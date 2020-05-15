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
#include "s3fs.h"
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <iostream>

auto get_duration(std::chrono::_V2::system_clock::time_point start, std::chrono::_V2::system_clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
}

// helper
std::unique_ptr<parquet::arrow::FileReader> get_column(std::unique_ptr<parquet::arrow::FileReader> reader, std::string col_name, int& col_index) {
  std::shared_ptr<arrow::Schema> schema;
  PARQUET_THROW_NOT_OK(reader->GetSchema(&schema));
  // std::cout << "schema->ToString:" << schema->ToString(true) << std::endl;
  auto field_names = schema->field_names();
  for (int i=0; i<schema->num_fields(); i++) {
    if (field_names[i] == col_name) {
      col_index = i;
      return reader;
    }
  }
  throw std::runtime_error("Field not found in schema");
}

// #1: Fully read in the file
void read_whole_file(std::unique_ptr<parquet::arrow::FileReader> reader)
{
  std::cout << "Reading parquet from s3 at once" << std::endl;

  std::shared_ptr<arrow::Table> table;
  PARQUET_THROW_NOT_OK(reader->ReadTable(&table));
  std::cout << "Loaded " << table->num_rows() << " rows in " << table->num_columns()
            << " columns." << std::endl;
}

// #2: Fully read in the file read only one row group for the column
void read_single_column_chunk(std::unique_ptr<parquet::arrow::FileReader> reader, std::string col_name)
{
  std::cout << "Reading first ColumnChunk of the first RowGroup"
            << std::endl;

  int col_index;
  reader = get_column(std::move(reader), col_name, col_index);
  std::cout << "id of " << col_name << " : " << col_index << std::endl;

  std::shared_ptr<arrow::ChunkedArray> array;
  PARQUET_THROW_NOT_OK(reader->RowGroup(0)->Column(col_index)->Read(&array)); // does not honor reader->set_use_threads(true);
  std::cout << "array->length:" << array->length() << std::endl;
  std::cout << "array->num_chunks:" << array->num_chunks() << std::endl;
  std::cout << "array->null_count:" << array->null_count() << std::endl;

  // PARQUET_THROW_NOT_OK(arrow::PrettyPrint(*array, 4, &std::cout));
  std::cout << std::endl;
}

// #3: Read an entire column
void read_single_column(std::unique_ptr<parquet::arrow::FileReader> reader, std::string col_name)
{
  std::cout << "Reading first ColumnChunk of the first RowGroup"
            << std::endl;

  int col_index;
  reader = get_column(std::move(reader), col_name, col_index);
  std::cout << "id of " << col_name << " : " << col_index << std::endl;
  
  std::shared_ptr<arrow::Table> table;
  auto t1 = std::chrono::high_resolution_clock::now();
  PARQUET_THROW_NOT_OK(reader->ReadTable({col_index}, &table));
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout << "read duration: " << get_duration(t1, t2) << std::endl;
  std::cout << "table->num_rows:" << table->num_rows() << std::endl;
  std::cout << "table->num_columns:" << table->num_columns() << std::endl;

  auto function_context = arrow::compute::FunctionContext();
  auto column_datum = arrow::compute::Datum(table->GetColumnByName("cpm"));
  arrow::compute::Datum result_datum;
  PARQUET_THROW_NOT_OK(arrow::compute::Sum(&function_context, column_datum, &result_datum));
  auto t3 = std::chrono::high_resolution_clock::now();
  std::cout << "sum duration: " << get_duration(t2, t3) << std::endl;
  std::cout << "sum:" << result_datum.scalar()->ToString() << std::endl;
  
  std::cout << std::endl;
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const &req)
{
  //// setup s3fs ////
  arrow::fs::S3Options options = arrow::fs::S3Options::Defaults();
  options.region = "eu-west-1";
  char *is_local = getenv("IS_LOCAL");
  if (is_local != NULL && strcmp(is_local, "true") == 0)
  {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  std::shared_ptr<arrow::fs::S3FileSystem> fs;
  PARQUET_ASSIGN_OR_THROW(fs, arrow::fs::S3FileSystem::Make(options));

  std::vector<std::string> file_names{
    "bb-test-data-dev/bid-large.parquet",
    // "bb-test-data-dev/bid-large-arrow-rg-500k.parquet",
    // "bb-test-data-dev/bid-large-arrow-rg-1m.parquet",
    // "bb-test-data-dev/bid-large-nolist-rg-2m.parquet",
    // "bb-test-data-dev/bid-large-nolist-rg-4m.parquet",
    // "bb-test-data-dev/bid-large-nolist-rg-full.parquet",
  };

  for(auto&& file_name: file_names) {
    std::cout << ">>>> file_name: " << file_name << std::endl;
    //// setup reader ////
    std::shared_ptr<arrow::io::RandomAccessFile> infile;
    PARQUET_ASSIGN_OR_THROW(
        infile, fs->OpenInputFile(file_name));

    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(
        parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));
    reader->set_use_threads(false);

    // std::cout << "reader->num_row_groups:" << reader->num_row_groups() << std::endl;
    // std::cout << "reader->num_rows:" << reader->parquet_reader()->metadata()->num_rows() << std::endl;
    // std::cout << "reader->size:" << reader->parquet_reader()->metadata()->size() << std::endl;
    // std::cout << "reader->version:" << reader->parquet_reader()->metadata()->version() << std::endl;

    //// read from s3 ////
    // read_whole_file(std::move(reader));
    // read_single_column_chunk(std::move(reader), "cpm");
    read_single_column(std::move(reader), "cpm");
  }

  fs->GetMetrics()->Print("resp_duration_ms");

  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

/** LAMBDA MAIN **/
int main()
{
  arrow::fs::S3GlobalOptions options;
  options.log_level = arrow::fs::S3LogLevel::Warn;
  PARQUET_THROW_NOT_OK(InitializeS3(options));
  char *is_local = getenv("IS_LOCAL");
  if (is_local != NULL && strcmp(is_local, "true") == 0)
  {
    std::cout << "IS_LOCAL=true" << std::endl;
    aws::lambda_runtime::invocation_response response = my_handler(aws::lambda_runtime::invocation_request());
    std::cout << response.get_payload() << std::endl;
  }
  else
  {
    aws::lambda_runtime::run_handler(my_handler);
  }
  return 0;
}
