
#include <arrow/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>
#include <future>

#include <iostream>


int64_t download_chunck(std::shared_ptr<arrow::io::RandomAccessFile> infile, int64_t start, int64_t nbbytes) {
  auto start_time = std::chrono::high_resolution_clock::now();
  std::shared_ptr<arrow::Buffer> buf;
  PARQUET_ASSIGN_OR_THROW(buf, infile->ReadAt(start, nbbytes));
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  return duration;
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

  //// setup reader ////
  std::shared_ptr<arrow::io::RandomAccessFile> infile;
  PARQUET_ASSIGN_OR_THROW(
      infile, fs->OpenInputFile("bb-test-data-dev/bid-large.parquet"));

  int64_t chunck_size = 250000;
  
  for (int nb_parallel = 1; nb_parallel < 16; nb_parallel+=2) {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::future<int64_t>> dl_futures;
    dl_futures.reserve(nb_parallel);
    for (int i=0; i<nb_parallel; i++) {
      auto fut = std::async(std::launch::async, download_chunck, infile, chunck_size*2, chunck_size);
      dl_futures.push_back(std::move(fut));
    }
    std::cout << "=> parallel:" << nb_parallel << std::endl;
    std::cout << "speed:";
    int64_t added_speed = 0;
    for (auto& dl_future: dl_futures) {
      auto local_duration = dl_future.get();
      auto local_speed = chunck_size/local_duration;
      added_speed+=local_speed;
      std::cout << local_speed << ",";
    }
    std::cout << std::endl;
    std::cout << "added_speed:" << added_speed << std::endl;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    std::cout << "true_speed:" << chunck_size*nb_parallel/total_duration << std::endl;
  }

  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

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