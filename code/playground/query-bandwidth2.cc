
#include <aws/lambda-runtime/runtime.h>

#include "aws-sdk.h"
#include "bootstrap.h"
#include "downloader.h"
#include "toolbox.h"

static int NB_CHUNCK = util::getenv_int("NB_CHUNCK", 12);
static int64_t CHUNK_SIZE = util::getenv_int("CHUNK_SIZE", 250000);

static aws::lambda_runtime::invocation_response my_handler(
    const aws::lambda_runtime::invocation_request& req, const S3Options& options) {
  auto synchronizer = std::make_shared<Synchronizer>();
  auto metrics_manager = std::make_shared<util::MetricsManager>();
  Downloader downloader{synchronizer, metrics_manager, options};
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
  std::cout << "downloaded_bytes:" << downloaded_bytes << std::endl;
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
  bool is_local = util::getenv_bool("IS_LOCAL", false);
  if (is_local) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  return bootstrap([&options](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(req, options);
  });
}
