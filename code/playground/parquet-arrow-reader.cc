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

#include <aws/lambda-runtime/runtime.h>

#include <iostream>

#include "bootstrap.h"
#include "cust_memory_pool.h"
#include "downloader.h"
#include "logger.h"
#include "parquet-helpers.h"
#include "partial-file.h"
#include "sdk-init.h"
#include "toolbox.h"

using namespace Buzz;

// extern char* je_arrow_malloc_conf;

static const int64_t MAX_CONCURRENT_DL = util::getenv_int("MAX_CONCURRENT_DL", 8);
static const int NB_CONN_INIT = util::getenv_int("NB_CONN_INIT", 1);
static const int64_t COLUMN_ID = util::getenv_int("COLUMN_ID", 16);
static const bool AS_DICT = util::getenv_bool("AS_DICT", true);
static const auto mem_pool = new CustomMemoryPool(arrow::default_memory_pool());
static const bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);

// Read a column chunck
Result<int64_t> read_column_chunck(std::shared_ptr<::arrow::io::RandomAccessFile> rg_file,
                                   std::shared_ptr<parquet::FileMetaData> file_metadata,
                                   int rg) {
  std::unique_ptr<parquet::arrow::FileReader> reader;
  parquet::arrow::FileReaderBuilder builder;
  parquet::ReaderProperties parquet_props(mem_pool);
  PARQUET_THROW_NOT_OK(builder.Open(rg_file, parquet_props, file_metadata));
  builder.memory_pool(mem_pool);
  auto arrow_props = parquet::ArrowReaderProperties();
  arrow_props.set_read_dictionary(COLUMN_ID, AS_DICT);
  builder.properties(arrow_props);
  PARQUET_THROW_NOT_OK(builder.Build(&reader));

  std::shared_ptr<arrow::ChunkedArray> array;
  PARQUET_THROW_NOT_OK(reader->RowGroup(rg)->Column(COLUMN_ID)->Read(&array));
  return array->length();
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req, const SdkOptions& options) {
  auto synchronizer = std::make_shared<Synchronizer>();
  auto metrics_manager = std::make_shared<MetricsManager>();
  metrics_manager->EnterPhase("wait_foot");
  // metrics_manager->Reset();
  auto downloader = std::make_shared<Downloader>(synchronizer, MAX_CONCURRENT_DL,
                                                 metrics_manager, options);

  S3Path file_path{"bb-test-data-dev", "bid-large.parquet"};

  auto file_metadata =
      GetMetadata(downloader, synchronizer, mem_pool, file_path, NB_CONN_INIT);

  metrics_manager->ExitPhase("wait_foot");
  std::cout << "col processed: " << file_metadata->schema()->Column(COLUMN_ID)->name()
            << std::endl;

  // Download column chuncks
  for (int i = 0; i < file_metadata->num_row_groups(); i++) {
    // TODO a more progressive scheduling of new connections
    DownloadColumnChunck(downloader, file_metadata, file_path, i, COLUMN_ID);
  }

  // Process chuncks
  int downloaded_chuncks = 0;
  int64_t rows_read = 0;
  metrics_manager->NewEvent("start_scheduler");
  while (downloaded_chuncks < file_metadata->num_row_groups()) {
    metrics_manager->EnterPhase("wait_dl");
    synchronizer->wait();
    metrics_manager->ExitPhase("wait_dl");
    auto col_chunck_files = GetChunckFiles(downloader);
    for (auto& col_chunck_file : col_chunck_files) {
      metrics_manager->EnterPhase("proc");
      metrics_manager->NewEvent("starting_proc");
      // read chunck
      rows_read += read_column_chunck(col_chunck_file.file, file_metadata,
                                      col_chunck_file.row_group)
                       .ValueOrDie();
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
