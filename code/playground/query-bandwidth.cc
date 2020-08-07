
#include <aws/lambda-runtime/runtime.h>

#include "bootstrap.h"
#include "downloader.h"
#include "logger.h"
#include "sdk-init.h"
#include "toolbox.h"

static int NB_CHUNCK = util::getenv_int("NB_CHUNCK", 12);
static int MAX_PARALLEL = util::getenv_int("MAX_PARALLEL", 12);
static int64_t CHUNK_SIZE = util::getenv_int("CHUNK_SIZE", 250000);
static int MEMORY_SIZE = util::getenv_int("AWS_LAMBDA_FUNCTION_MEMORY_SIZE", 0);
static bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);

static aws::lambda_runtime::invocation_response my_handler(
    const aws::lambda_runtime::invocation_request& req, const SdkOptions& options) {
  auto synchronizer = std::make_shared<Synchronizer>();
  auto metrics_manager = std::make_shared<util::MetricsManager>();
  // metrics_manager->Reset();
  Downloader downloader{synchronizer, MAX_PARALLEL, metrics_manager, options};
  // init connections
  auto nb_inits = MAX_PARALLEL;
  downloader.InitConnections("bb-test-data-dev", nb_inits);
  int inits_completed = 0;
  while (inits_completed < nb_inits) {
    // wait for all inits to be finised before moving to dl
    // in the dispatcher loop, we'll move forward as the metada request responded
    synchronizer->wait();
    auto results = downloader.ProcessResponses();
    inits_completed += results.size();
  }
  // start download
  auto start_time = util::time::now();
  for (int i = 0; i < NB_CHUNCK; i++) {
    downloader.ScheduleDownload({i * CHUNK_SIZE,
                                 (i + 1) * CHUNK_SIZE - 1,
                                 {"bb-test-data-dev", "bid-large.parquet"}});
  }
  int downloaded_chuncks = 0;
  int downloaded_bytes = 0;
  while (downloaded_chuncks < NB_CHUNCK) {
    synchronizer->wait();
    auto results = downloader.ProcessResponses();
    for (auto& result : results) {
      // if (result.status().message() == STATUS_ABORTED.message()) {
      //   inits_aborted++;
      //   continue;
      // }
      auto response = result.ValueOrDie();
      // if (response.request.range_start == 0 && response.request.range_end == 0) {
      //   inits_completed++;
      //   continue;
      // }
      downloaded_chuncks++;
      downloaded_bytes += response.raw_data->size();
    }
  }
  auto end_time = util::time::now();
  auto total_duration = util::get_duration_ms(start_time, end_time);
  metrics_manager->NewEvent("handler_end");
  // logging all results
  metrics_manager->Print();
  auto entry = Buzz::logger::NewEntry("query_bandwidth");
  entry.IntField("NB_CHUNCK", NB_CHUNCK);
  entry.IntField("MAX_PARALLEL", MAX_PARALLEL);
  entry.IntField("CHUNK_SIZE", CHUNK_SIZE);
  entry.IntField("MEMORY_SIZE", MEMORY_SIZE);
  entry.IntField("downloaded_bytes", downloaded_bytes);
  entry.IntField("duration_ms", total_duration);
  entry.IntField("inits_completed", inits_completed);
  // entry.IntField("inits_aborted", inits_aborted);
  entry.FloatField("speed_MBpS", downloaded_bytes / 1000000. / (total_duration / 1000.));
  entry.Log();
  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

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
