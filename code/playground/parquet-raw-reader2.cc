// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements. See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership. The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/api/reader.h>
#include <parquet/exception.h>

#include <iostream>

#include "bootstrap.h"
#include "cust_memory_pool.h"
#include "downloader.h"
#include "logger.h"
#include "partial-file.h"
#include "sdk-init.h"
#include "stats.h"
#include "toolbox.h"

// extern char* je_arrow_malloc_conf;

static const int MAX_CONCURRENT_DL = util::getenv_int("MAX_CONCURRENT_DL", 8);
static const int NB_CONN_INIT = util::getenv_int("NB_CONN_INIT", 1);
static const int64_t COLUMN_ID = util::getenv_int("COLUMN_ID", 16);
static const auto mem_pool = new arrow::CustomMemoryPool(arrow::default_memory_pool());
static const bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);

// Read a column chunck
int64_t read_column_chunck(std::unique_ptr<parquet::ParquetFileReader> reader, int rg) {
  // reader->metadata()->schema()->Column(col_index)->logical_type();
  auto untyped_col = reader->RowGroup(rg)->Column(COLUMN_ID);
  // untyped_col->type()
  // BOOLEAN = 0,
  // INT32 = 1,
  // INT64 = 2,
  // INT96 = 3,
  // FLOAT = 4,
  // DOUBLE = 5,
  // BYTE_ARRAY = 6,
  // FIXED_LEN_BYTE_ARRAY = 7,
  // UNDEFINED = 8
  auto* typed_reader = static_cast<parquet::ByteArrayReader*>(untyped_col.get());
  constexpr int batch_size = 1024 * 2;
  int64_t total_values_read = 0;
  // this should be aligned
  uint8_t* values;
  mem_pool->Allocate(batch_size * sizeof(parquet::ByteArray), &values);
  auto values_casted = reinterpret_cast<parquet::ByteArray*>(values);
  // util::CountStat<parquet::ByteArray> count_stat;
  while (typed_reader->HasNext()) {
    int64_t values_read = 0;
    typed_reader->ReadBatch(batch_size, nullptr, nullptr, values_casted, &values_read);
    // count_stat.Add(values_casted, values_read);
    total_values_read += values_read;
  }
  // count_stat.Print();
  return total_values_read;
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req, const SdkOptions& options) {
  auto wait_for_foot_start = util::time::now();
  auto synchronizer = std::make_shared<Synchronizer>();
  auto metrics_manager = std::make_shared<util::MetricsManager>();
  // metrics_manager->Reset();
  Downloader downloader{synchronizer, MAX_CONCURRENT_DL, metrics_manager, options};

  downloader.ScheduleDownload(
      {std::nullopt, 64 * 1024, {"bb-test-data-dev", "bid-large.parquet"}});

  downloader.InitConnections("bb-test-data-dev", NB_CONN_INIT);

  int inits_aborted = 0;
  int inits_completed = 0;
  DownloadResponse footer_response{{}, nullptr, 0};
  while (footer_response.file_size == 0) {
    synchronizer->wait();
    auto results = downloader.ProcessResponses();
    for (auto& result : results) {
      auto response = result.ValueOrDie();
      if (response.request.range_start.value_or(0) == 0 &&
          response.request.range_end == 0) {
        inits_completed++;
      } else {
        footer_response = response;
      }
    }
  }
  auto wait_for_foot = util::get_duration_ms(wait_for_foot_start, util::time::now());
  auto footer_start_pos = footer_response.file_size - footer_response.request.range_end;
  std::vector<Buzz::FileChunck> footer_chuncks{
      {footer_start_pos, footer_response.raw_data}};
  auto footer_file =
      std::make_shared<Buzz::PartialFile>(footer_chuncks, footer_response.file_size);

  //// setup reader ////
  parquet::ReaderProperties props(mem_pool);
  std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
      parquet::ParquetFileReader::Open(footer_file, props, nullptr);

  // Get the File MetaData
  std::shared_ptr<parquet::FileMetaData> file_metadata = parquet_reader->metadata();
  std::cout << "file_metadata->num_rows:" << file_metadata->num_rows() << std::endl;
  std::cout << "col processed: " << file_metadata->schema()->Column(COLUMN_ID)->name()
            << std::endl;

  std::map<int64_t, int> rg_start_map;

  for (int i = 0; i < file_metadata->num_row_groups(); i++) {
    // TODO a more progressive scheduling of new connections
    auto col_chunck_meta = file_metadata->RowGroup(i)->ColumnChunk(COLUMN_ID);
    auto col_chunck_start = col_chunck_meta->file_offset();
    downloader.ScheduleDownload(
        {col_chunck_start,
         col_chunck_start + col_chunck_meta->total_compressed_size(),
         {"bb-test-data-dev", "bid-large.parquet"}});
    rg_start_map.emplace(col_chunck_start, i);
  }

  int downloaded_chuncks = 0;
  int64_t downloaded_bytes = 0;
  int64_t rows_read = 0;
  int64_t wait_for_dl = 0;
  std::vector<int64_t> wait_durations;
  wait_durations.reserve(file_metadata->num_row_groups());
  std::vector<int64_t> processing_durations;
  processing_durations.reserve(file_metadata->num_row_groups());
  metrics_manager->NewEvent("start_scheduler");
  while (downloaded_chuncks < file_metadata->num_row_groups()) {
    auto wait_for_dl_start = util::time::now();
    synchronizer->wait();
    auto wait_duration = util::get_duration_ms(wait_for_dl_start, util::time::now());
    wait_durations.push_back(wait_duration);
    wait_for_dl += wait_duration;
    auto results = downloader.ProcessResponses();
    for (auto& result : results) {
      auto start_processing = util::time::now();
      metrics_manager->NewEvent("starting_proc");
      if (result.status().message() == STATUS_ABORTED.message()) {
        inits_aborted++;
        continue;
      }
      auto response = result.ValueOrDie();
      if (response.request.range_start.value_or(0) == 0 &&
          response.request.range_end == 0) {
        inits_completed++;
        continue;
      }
      auto rg = rg_start_map[response.request.range_start.value()];
      std::vector<Buzz::FileChunck> rg_chuncks{
          {response.request.range_start.value(), response.raw_data}};
      auto rg_file = std::make_shared<Buzz::PartialFile>(rg_chuncks, response.file_size);
      //// setup reader ////
      parquet::ReaderProperties props(mem_pool);
      std::unique_ptr<parquet::ParquetFileReader> rg_reader =
          parquet::ParquetFileReader::Open(rg_file, props, file_metadata);
      rows_read += read_column_chunck(std::move(rg_reader), rg);
      downloaded_chuncks++;
      downloaded_bytes += response.raw_data->size();
      processing_durations.push_back(
          util::get_duration_ms(start_processing, util::time::now()));
    }
  }
  metrics_manager->NewEvent("processings_finished");

  std::cout << "downloaded_chuncks:" << downloaded_chuncks
            << "/downloaded_bytes:" << downloaded_bytes << "/rows_read:" << rows_read
            << std::endl;
  metrics_manager->Print();

  std::cout << "wait_dur:";
  for (auto wait_dur : wait_durations) {
    std::cout << wait_dur << ",";
  }
  std::cout << std::endl;
  std::cout << "proc_dur:";
  for (auto proc_dur : processing_durations) {
    std::cout << proc_dur << ",";
  }
  std::cout << std::endl;

  auto entry = Buzz::logger::NewEntry("wait_s3");
  entry.IntField("nb_init", NB_CONN_INIT);
  entry.IntField("footer", wait_for_foot);
  entry.IntField("dl", wait_for_dl);
  entry.IntField("total", wait_for_foot + wait_for_dl);
  entry.Log();

  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

/** LAMBDA MAIN **/
int main() {
  InitializeAwsSdk(AwsSdkLogLevel::Off);
  // init s3 client
  SdkOptions options;
  options.region = "eu-west-1";
  if (IS_LOCAL) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  bootstrap([&options](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(req, options);
  });
  // this is mainly usefull to avoid Valgrind errors as Lambda do not guaranty the
  // execution of this code before killing the container
  FinalizeAwsSdk();
}
