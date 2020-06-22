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

#include <future>
#include <iostream>

#include "cust_memory_pool.h"
#include "s3fs-forked.h"
#include "scheduler.h"
#include "stats.h"
#include "toolbox.h"

// extern char* je_arrow_malloc_conf;

static const int64_t MAX_CONCURRENT_DL = util::getenv_int("MAX_CONCURRENT_DL", 8);
static const int64_t MAX_CONCURRENT_PROC = util::getenv_int("MAX_CONCURRENT_PROC", 1);
static const int64_t COLUMN_ID = util::getenv_int("COLUMN_ID", 16);
static const auto mem_pool = new arrow::CustomMemoryPool(arrow::default_memory_pool());

// Read an entire column chunck by chunck
void read_single_column_parallel(std::unique_ptr<parquet::ParquetFileReader> reader,
                                 std::shared_ptr<arrow::fs::fork::S3FileSystem> fs,
                                 int64_t col_index) {
  auto col_name = reader->metadata()->schema()->Column(col_index)->name();
  std::cout << "col processed: " << col_name << std::endl;
  // start rowgroup read at the same time
  std::shared_ptr<parquet::ParquetFileReader> shared_reader(std::move(reader));
  std::vector<std::future<int64_t>> rg_futures;
  rg_futures.reserve(shared_reader->metadata()->num_row_groups());
  // shared_reader->metadata()->schema()->Column(col_index)->logical_type();
  for (int i = 0; i < shared_reader->metadata()->num_row_groups(); i++) {
    auto fut = std::async(std::launch::async, [col_index, i, shared_reader, fs]() {
      fs->GetMetrics()->NewEvent("read_start");
      fs->GetResourceScheduler()->RegisterThreadForSync();
      auto untyped_col = shared_reader->RowGroup(i)->Column(col_index);
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
      util::CountStat<parquet::ByteArray> count_stat;
      while (typed_reader->HasNext()) {
        int64_t values_read = 0;
        typed_reader->ReadBatch(batch_size, nullptr, nullptr, values_casted,
                                &values_read);
        count_stat.Add(values_casted, values_read);
        total_values_read += values_read;
      }
      fs->GetResourceScheduler()->NotifyProcessingDone();
      fs->GetMetrics()->NewEvent("read_end");
      count_stat.Print();
      return total_values_read;
    });
    rg_futures.push_back(std::move(fut));
  }
  // collect rowgroup results
  std::vector<int64_t> values_read_by_rg;
  values_read_by_rg.reserve(shared_reader->metadata()->num_row_groups());
  for (auto& rg_future : rg_futures) {
    auto values_read = rg_future.get();
    values_read_by_rg.push_back(values_read);
  }

  std::cout << "values_read_by_rg:";
  for (auto rows : values_read_by_rg) {
    std::cout << rows << ",";
  }
  std::cout << std::endl;
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req) {
  auto scheduler =
      std::make_shared<util::ResourceScheduler>(MAX_CONCURRENT_DL, MAX_CONCURRENT_PROC);
  auto metrics = std::make_shared<util::MetricsManager>();

  //// setup s3fs ////
  arrow::fs::fork::S3Options options = arrow::fs::fork::S3Options::Defaults();
  options.region = "eu-west-1";
  char* is_local = getenv("IS_LOCAL");
  if (is_local != NULL && strcmp(is_local, "true") == 0) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  PARQUET_ASSIGN_OR_THROW(
      auto fs, arrow::fs::fork::S3FileSystem::Make(options, scheduler, metrics));

  std::vector<std::string> file_names{
      "bb-test-data-dev/bid-large.parquet",
      // "bb-test-data-dev/bid-large-arrow-rg-500k.parquet",
      // "bb-test-data-dev/bid-large-arrow-rg-1m.parquet",
      // "bb-test-data-dev/bid-large-nolist-rg-2m.parquet",
      // "bb-test-data-dev/bid-large-nolist-rg-4m.parquet",
      // "bb-test-data-dev/bid-large-nolist-rg-full.parquet",
  };

  for (auto&& file_name : file_names) {
    std::cout << ">>>> file_name: " << file_name << std::endl;
    //// setup reader ////
    PARQUET_ASSIGN_OR_THROW(auto infile, fs->OpenInputFile(file_name));
    parquet::ReaderProperties props(mem_pool);

    std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
        parquet::ParquetFileReader::Open(infile, props, nullptr);

    // Get the File MetaData
    std::shared_ptr<parquet::FileMetaData> file_metadata = parquet_reader->metadata();
    std::cout << "file_metadata->num_rows:" << file_metadata->num_rows() << std::endl;

    //// read from s3 ////
    read_single_column_parallel(std::move(parquet_reader), fs, COLUMN_ID);
    std::cout << "HUGE_ALLOC_THRESHOLD_BYTES:" << arrow::HUGE_ALLOC_THRESHOLD_BYTES
              << std::endl;
    std::cout << "arrow::default_memory_pool()->bytes_allocated():"
              << arrow::default_memory_pool()->bytes_allocated() << std::endl;
    std::cout << "mem_pool->bytes_allocated():" << mem_pool->bytes_allocated()
              << std::endl;
    std::cout << "arrow::default_memory_pool()->max_memory():"
              << arrow::default_memory_pool()->max_memory() << std::endl;
    std::cout << "mem_pool->copied_bytes():" << mem_pool->copied_bytes() << std::endl;
  }

  fs->GetMetrics()->Print();

  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

/** LAMBDA MAIN **/
int main() {
  arrow::fs::fork::S3GlobalOptions options;
  options.log_level = arrow::fs::fork::S3LogLevel::Warn;
  PARQUET_THROW_NOT_OK(InitializeS3(options));
  if (util::getenv_bool("IS_LOCAL", false)) {
    std::cout << "IS_LOCAL=true" << std::endl;
    aws::lambda_runtime::invocation_response response =
        my_handler(aws::lambda_runtime::invocation_request());
    std::cout << response.get_payload() << std::endl;
  } else {
    aws::lambda_runtime::run_handler(my_handler);
  }
  return 0;
}
