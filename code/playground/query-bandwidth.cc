
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <future>
#include <iostream>

#include "bootstrap.h"
#include "s3fs-forked.h"
#include "toolbox.h"

static int NB_PARALLEL = util::getenv_int("NB_PARALLEL", 12);
static int64_t CHUNK_SIZE = util::getenv_int("CHUNK_SIZE", 250000);

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
    std::shared_ptr<arrow::fs::fork::S3FileSystem> fs,
    aws::lambda_runtime::invocation_request const& req) {
  std::vector<std::string> file_names{
      "bb-test-data-dev/bid-large.parquet",
      // "bb-test-data-dev/bid-large-bis.parquet",
  };

  for (auto file_name : file_names) {
    std::shared_ptr<arrow::io::RandomAccessFile> infile;
    fs->GetMetrics()->NewEvent("open_input_file");
    PARQUET_ASSIGN_OR_THROW(infile, fs->OpenInputFile(file_name));
    fs->GetMetrics()->NewEvent("input_file_opened");
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::future<int64_t>> dl_futures;
    dl_futures.reserve(NB_PARALLEL);
    for (int i = 0; i < NB_PARALLEL; i++) {
      auto fut = std::async(std::launch::async, download_chunck, infile, i * CHUNK_SIZE,
                            CHUNK_SIZE);
      dl_futures.push_back(std::move(fut));
    }
    fs->GetMetrics()->NewEvent("future_creation_done");
    std::cout << "=> parallel:" << NB_PARALLEL << std::endl;
    // std::cout << "speed:";
    int64_t added_speed{0};
    for (auto& dl_future : dl_futures) {
      auto local_duration = dl_future.get();
      auto local_speed = CHUNK_SIZE / local_duration;  // (to µs / to MB)
      added_speed += local_speed;
      // std::cout << local_speed << ",";
    }
    // std::cout << std::endl;
    // std::cout << "added_speed:" << added_speed << std::endl;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = util::get_duration_ms(start_time, end_time);
    std::cout << "duration_ms:" << total_duration << "/speed_MBpS:"
              << CHUNK_SIZE / 1000000. * NB_PARALLEL / (total_duration / 1000.)
              << std::endl;
  }

  fs->GetMetrics()->Print();

  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

int main() {
  // init SDK
  arrow::fs::fork::S3GlobalOptions global_options;
  global_options.log_level = arrow::fs::fork::S3LogLevel::Warn;
  PARQUET_THROW_NOT_OK(InitializeS3(global_options));
  // init s3 client
  arrow::fs::fork::S3Options options = arrow::fs::fork::S3Options::Defaults();
  options.region = "eu-west-1";
  bool is_local = util::getenv_bool("IS_LOCAL", false);
  if (is_local) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  std::shared_ptr<arrow::fs::fork::S3FileSystem> fs;
  auto scheduler = std::make_shared<util::ResourceScheduler>(1, 1);
  auto metrics = std::make_shared<util::MetricsManager>();
  PARQUET_ASSIGN_OR_THROW(
      fs, arrow::fs::fork::S3FileSystem::Make(options, scheduler, metrics));
  return bootstrap([fs](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(fs, req);
  });
}
