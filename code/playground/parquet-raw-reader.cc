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
#include "parquet-helper.h"
#include "partial-file.h"
#include "sdk-init.h"
#include "stats.h"
#include "toolbox.h"

using namespace Buzz;

// extern char* je_arrow_malloc_conf;

static const int MAX_CONCURRENT_DL = util::getenv_int("MAX_CONCURRENT_DL", 8);
static const int NB_CONN_INIT = util::getenv_int("NB_CONN_INIT", 1);
static const int64_t COLUMN_ID = util::getenv_int("COLUMN_ID", 16);
static const auto mem_pool = new CustomMemoryPool(arrow::default_memory_pool());
static const bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);

// Read a column chunck
int64_t read_column_chunck(std::shared_ptr<PartialFile> rg_file,
                           std::shared_ptr<parquet::FileMetaData> file_metadata, int rg) {
  parquet::ReaderProperties props(mem_pool);
  std::unique_ptr<parquet::ParquetFileReader> reader =
      parquet::ParquetFileReader::Open(rg_file, props, file_metadata);
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
  // CountStat<parquet::ByteArray> count_stat;
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
  auto synchronizer = std::make_shared<Synchronizer>();
  auto metrics_manager = std::make_shared<MetricsManager>();
  // metrics_manager->Reset();
  metrics_manager->EnterPhase("wait_foot");
  auto downloader = std::make_shared<Downloader>(synchronizer, MAX_CONCURRENT_DL,
                                                 metrics_manager, options);

  ParquetHelper parquet_helper{downloader};

  S3Path file_path{"bb-test-data-dev", "bid-large.parquet"};

  auto file_metadata =
      parquet_helper.GetMetadata(synchronizer, mem_pool, file_path, NB_CONN_INIT);

  metrics_manager->ExitPhase("wait_foot");

  // Download column chuncks
  for (int i = 0; i < file_metadata->num_row_groups(); i++) {
    // TODO a more progressive scheduling of new connections
    parquet_helper.DownloadColumnChunck(file_metadata, file_path, i, COLUMN_ID);
  }

  // Process chuncks
  int downloaded_chuncks = 0;
  int64_t rows_read = 0;
  metrics_manager->NewEvent("start_scheduler");
  while (downloaded_chuncks < file_metadata->num_row_groups()) {
    metrics_manager->EnterPhase("wait_dl");
    synchronizer->wait();
    metrics_manager->ExitPhase("wait_dl");
    auto col_chunck_files = parquet_helper.GetChunckFiles();
    for (auto& col_chunck_file : col_chunck_files) {
      metrics_manager->EnterPhase("proc");
      metrics_manager->NewEvent("starting_proc");
      // read chunck
      rows_read += read_column_chunck(col_chunck_file.file, file_metadata,
                                      col_chunck_file.row_group);
      downloaded_chuncks++;
      metrics_manager->ExitPhase("proc");
    }
  }
  metrics_manager->NewEvent("processings_finished");

  std::cout << "downloaded_chuncks:" << downloaded_chuncks << "/rows_read:" << rows_read
            << std::endl;
  metrics_manager->Print();

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
