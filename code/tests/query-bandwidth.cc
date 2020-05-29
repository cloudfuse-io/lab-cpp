
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <future>
#include <iostream>

#include "s3fs-forked.h"
#include "various.h"

int64_t download_chunck(std::shared_ptr<arrow::io::RandomAccessFile> infile,
                        int64_t start, int64_t nbbytes) {
  auto start_time = std::chrono::high_resolution_clock::now();
  std::shared_ptr<arrow::Buffer> buf;
  PARQUET_ASSIGN_OR_THROW(buf, infile->ReadAt(start, nbbytes));
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = util::get_duration_ms(start_time, end_time);
  return duration;
}

static aws::lambda_runtime::invocation_response my_handler(
    std::shared_ptr<arrow::fs::S3FileSystem> fs,
    aws::lambda_runtime::invocation_request const& req) {
  std::vector<std::string> file_names{
      "bb-test-data-dev/bid-large.parquet",
      "bb-test-data-dev/bid-large-bis.parquet",
  };

  int64_t chunck_size{250000};
  for (auto file_name : file_names) {
    int nb_parallel{20};
    std::shared_ptr<arrow::io::RandomAccessFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, fs->OpenInputFile(file_name));
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::future<int64_t>> dl_futures;
    dl_futures.reserve(nb_parallel);
    for (int i = 0; i < nb_parallel; i++) {
      auto fut = std::async(std::launch::async, download_chunck, infile, i * chunck_size,
                            chunck_size);
      dl_futures.push_back(std::move(fut));
    }
    std::cout << "=> parallel:" << nb_parallel << std::endl;
    // std::cout << "speed:";
    int64_t added_speed{0};
    for (auto& dl_future : dl_futures) {
      auto local_duration = dl_future.get();
      auto local_speed = chunck_size / local_duration;  // (to µs / to MB)
      added_speed += local_speed;
      // std::cout << local_speed << ",";
    }
    // std::cout << std::endl;
    // std::cout << "added_speed:" << added_speed << std::endl;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = util::get_duration_ms(start_time, end_time);
    std::cout << "duration_ms:" << total_duration / 1000
              << "/speed_MBpS:" << chunck_size * nb_parallel / total_duration
              << std::endl;
  }

  fs->GetMetrics()->Print();

  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

int main() {
  // init SDK
  arrow::fs::S3GlobalOptions global_options;
  global_options.log_level = arrow::fs::S3LogLevel::Warn;
  PARQUET_THROW_NOT_OK(InitializeS3(global_options));
  // init s3 client
  arrow::fs::S3Options options = arrow::fs::S3Options::Defaults();
  options.region = "eu-west-1";
  bool is_local = getenv("IS_LOCAL") != NULL && strcmp(getenv("IS_LOCAL"), "true") == 0;
  if (is_local) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  std::shared_ptr<arrow::fs::S3FileSystem> fs;
  PARQUET_ASSIGN_OR_THROW(fs, arrow::fs::S3FileSystem::Make(options));
  auto handler_lambda = [fs](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(fs, req);
  };
  if (is_local) {
    aws::lambda_runtime::invocation_response response =
        handler_lambda(aws::lambda_runtime::invocation_request());
    std::cout << response.get_payload() << std::endl;
  } else {
    aws::lambda_runtime::run_handler(handler_lambda);
  }
  return 0;
}
