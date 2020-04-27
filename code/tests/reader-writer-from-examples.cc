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
#include <arrow/filesystem/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <iostream>

// #2: Fully read in the file
void read_whole_file()
{
  std::cout << "Reading parquet from s3 at once" << std::endl;

  arrow::fs::S3Options options = arrow::fs::S3Options::Defaults();
  options.region = "eu-west-1";
  std::shared_ptr<arrow::fs::S3FileSystem> fs;
  PARQUET_ASSIGN_OR_THROW(fs, arrow::fs::S3FileSystem::Make(options));
  std::shared_ptr<arrow::io::RandomAccessFile> infile;

  PARQUET_ASSIGN_OR_THROW(
      // infile, fs->OpenInputFile("temp-pub-buck/parquet-arrow-example.parquet"));
      infile, fs->OpenInputFile("bb-test-data-dev/bid-small.parquet"));

  std::unique_ptr<parquet::arrow::FileReader> reader;
  PARQUET_THROW_NOT_OK(
      parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));
  std::shared_ptr<arrow::Table> table;
  PARQUET_THROW_NOT_OK(reader->ReadTable(&table));
  std::cout << "Loaded " << table->num_rows() << " rows in " << table->num_columns()
            << " columns." << std::endl;
}

void read_single_column_chunk()
{
  std::cout << "Reading first ColumnChunk of the first RowGroup"
            << std::endl;

  arrow::fs::S3Options options = arrow::fs::S3Options::Defaults();
  options.region = "eu-west-1";
  std::shared_ptr<arrow::fs::S3FileSystem> fs;
  PARQUET_ASSIGN_OR_THROW(fs, arrow::fs::S3FileSystem::Make(options));
  std::shared_ptr<arrow::io::RandomAccessFile> infile;

  PARQUET_ASSIGN_OR_THROW(
      // infile, fs->OpenInputFile("temp-pub-buck/parquet-arrow-example.parquet"));
      infile, fs->OpenInputFile("bb-test-data-dev/bid-small.parquet"));

  std::unique_ptr<parquet::arrow::FileReader> reader;
  PARQUET_THROW_NOT_OK(
      parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));
  std::shared_ptr<arrow::ChunkedArray> array;
  PARQUET_THROW_NOT_OK(reader->RowGroup(0)->Column(0)->Read(&array));
  std::cout << "array->length:" << array->length() << std::endl;
  std::cout << "array->num_chunks:" << array->num_chunks() << std::endl;
  std::cout << "array->null_count:" << array->null_count() << std::endl;

  PARQUET_THROW_NOT_OK(arrow::PrettyPrint(*array, 4, &std::cout));
  std::cout << std::endl;
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const &req)
{
  std::cout << "Hello from C++" << std::endl;
  // read_whole_file();
  read_single_column_chunk();
  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

/** LAMBDA MAIN **/
int main()
{
  arrow::fs::S3GlobalOptions options;
  options.log_level = arrow::fs::S3LogLevel::Debug;
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

/** CLASSICAL MAIN **/
// int main(int argc, char** argv) {
//   aws::lambda_runtime::invocation_request req;
//   my_handler(req);
// }