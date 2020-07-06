
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <future>
#include <iostream>

#include "bootstrap.h"
#include "logger.h"
#include "s3fs-forked.h"
#include "toolbox.h"

static int NB_PARALLEL = util::getenv_int("NB_PARALLEL", 12);
static int64_t CHUNK_SIZE = util::getenv_int("CHUNK_SIZE", 250000);
static bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);
static int MEMORY_SIZE = util::getenv_int("AWS_LAMBDA_FUNCTION_MEMORY_SIZE", 0);
static util::Logger LOGGER = util::Logger(IS_LOCAL);
static int64_t CONTAINER_RUNS = 0;

int64_t download_chunck(std::shared_ptr<arrow::io::RandomAccessFile> infile,
                        int64_t start, int64_t nbbytes) {
  std::shared_ptr<arrow::Buffer> buf;
  PARQUET_ASSIGN_OR_THROW(buf, infile->ReadAt(start, nbbytes));
  return buf->size();
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req,
    const arrow::fs::fork::S3Options& options) {
  CONTAINER_RUNS++;
  // init fs
  std::shared_ptr<arrow::fs::fork::S3FileSystem> fs;
  auto scheduler = std::make_shared<util::ResourceScheduler>(1, 1);
  auto metrics = std::make_shared<util::MetricsManager>();
  PARQUET_ASSIGN_OR_THROW(
      fs, arrow::fs::fork::S3FileSystem::Make(options, scheduler, metrics));
  // init file (calls head)
  auto file_name = "bb-test-data-dev/bid-large.parquet";
  std::shared_ptr<arrow::io::RandomAccessFile> infile;
  fs->GetMetrics()->NewEvent("open_input_file");
  PARQUET_ASSIGN_OR_THROW(infile, fs->OpenInputFile(file_name));
  fs->GetMetrics()->NewEvent("input_file_opened");
  // get chuncks
  auto start_time = std::chrono::high_resolution_clock::now();
  std::vector<std::future<int64_t>> dl_futures;
  dl_futures.reserve(NB_PARALLEL);
  for (int i = 0; i < NB_PARALLEL; i++) {
    auto fut = std::async(std::launch::async, download_chunck, infile, i * CHUNK_SIZE,
                          CHUNK_SIZE);
    dl_futures.push_back(std::move(fut));
  }
  fs->GetMetrics()->NewEvent("future_creation_done");
  int64_t downloaded_bytes{0};
  for (auto& dl_future : dl_futures) {
    downloaded_bytes += dl_future.get();
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  auto total_duration = util::get_duration_ms(start_time, end_time);
  auto entry = LOGGER.NewEntry("query_bandwidth");
  entry.IntField("CONTAINER_RUNS", CONTAINER_RUNS);
  entry.IntField("CHUNK_SIZE", CHUNK_SIZE);
  entry.IntField("MEMORY_SIZE", MEMORY_SIZE);
  entry.IntField("NB_PARALLEL", NB_PARALLEL);
  entry.IntField("downloaded_bytes", downloaded_bytes);
  entry.IntField("duration_ms", total_duration);
  entry.FloatField("speed_MBpS", downloaded_bytes / 1000000. / (total_duration / 1000.));
  entry.Log();

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
  if (IS_LOCAL) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }

  return bootstrap([&options](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(req, options);
  });
}
