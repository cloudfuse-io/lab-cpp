
#include <aws/lambda-runtime/runtime.h>

#include "aws-sdk.h"
#include "bootstrap.h"
#include "downloader.h"
#include "logger.h"
#include "toolbox.h"

static int NB_CHUNCK = util::getenv_int("NB_CHUNCK", 12);
static int MAX_PARALLEL = util::getenv_int("MAX_PARALLEL", 12);
static int64_t CHUNK_SIZE = util::getenv_int("CHUNK_SIZE", 250000);
static int MEMORY_SIZE = util::getenv_int("AWS_LAMBDA_FUNCTION_MEMORY_SIZE", 0);
static bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);
static util::Logger LOGGER = util::Logger(IS_LOCAL);
static int64_t CONTAINTER_RUNS = 0;

static aws::lambda_runtime::invocation_response my_handler(
    const aws::lambda_runtime::invocation_request& req, const S3Options& options) {
  CONTAINTER_RUNS++;
  auto synchronizer = std::make_shared<Synchronizer>();
  auto metrics_manager = std::make_shared<util::MetricsManager>();
  Downloader downloader{synchronizer, MAX_PARALLEL, metrics_manager, options};
  auto start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < NB_CHUNCK; i++) {
    downloader.ScheduleDownload({i * CHUNK_SIZE,
                                 (i + 1) * CHUNK_SIZE - 1,
                                 {"bb-test-data-dev", "bid-large.parquet"}});
  }
  metrics_manager->NewEvent("downloads_scheduled");
  int downloaded_chuncks = 0;
  int downloaded_bytes = 0;
  while (downloaded_chuncks < NB_CHUNCK) {
    synchronizer->wait();
    auto results = downloader.ProcessResponses();
    downloaded_chuncks += results.size();
    for (auto& result : results) {
      downloaded_bytes += result.ValueOrDie().raw_data->size();
    }
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  auto total_duration = util::get_duration_ms(start_time, end_time);
  auto entry = LOGGER.NewEntry("bandwidth_stats");
  entry.IntField("CONTAINTER_RUNS", CONTAINTER_RUNS);
  entry.IntField("NB_CHUNCK", NB_CHUNCK);
  entry.IntField("MAX_PARALLEL", MAX_PARALLEL);
  entry.IntField("CHUNK_SIZE", CHUNK_SIZE);
  entry.IntField("MEMORY_SIZE", MEMORY_SIZE);
  entry.IntField("downloaded_bytes", downloaded_bytes);
  entry.IntField("duration_ms", total_duration);
  entry.FloatField("speed_MBpS", downloaded_bytes / 1000000. / (total_duration / 1000.));
  entry.Log();
  metrics_manager->NewEvent("handler_end");
  metrics_manager->Print();
  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

int main() {
  // init SDK
  InitializeAwsSdk(AwsSdkLogLevel::Warn);
  // init s3 client
  S3Options options;
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
