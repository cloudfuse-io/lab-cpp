#include <aws/core/client/RetryStrategy.h>
#include <aws/lambda/LambdaClient.h>
#include <aws/lambda/model/InvokeRequest.h>

#include "async_queue.h"
#include "sdk-init.h"
#include "toolbox.h"

using namespace Buzz;

static bool IS_LOCAL = false;  // util::getenv_bool("IS_LOCAL", false);
static int NB_PARALLEL = util::getenv_int("NB_PARALLEL", 1);
static int NB_INVOKE = util::getenv_int("NB_INVOKE", 1);
static const char* BEE_FUNCTION_NAME =
    util::getenv("BEE_FUNCTION_NAME", "cloudfuse-lab-cpp-generic-playground-static-dev");

namespace {
class ConnectRetryStrategy : public Aws::Client::RetryStrategy {
 public:
  static const int32_t kDefaultRetryInterval = 200;     /* milliseconds */
  static const int32_t kDefaultMaxRetryDuration = 6000; /* milliseconds */

  explicit ConnectRetryStrategy(int32_t retry_interval = kDefaultRetryInterval,
                                int32_t max_retry_duration = kDefaultMaxRetryDuration)
      : retry_interval_(retry_interval), max_retry_duration_(max_retry_duration) {}

  bool ShouldRetry(const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
                   long attempted_retries) const override {  // NOLINT
    return attempted_retries * retry_interval_ < max_retry_duration_;
  }

  long CalculateDelayBeforeNextRetry(  // NOLINT
      const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
      long attempted_retries) const override {  // NOLINT
    return retry_interval_;
  }

 protected:
  int32_t retry_interval_;
  int32_t max_retry_duration_;
};
}  // namespace

struct InvokeResponse {};

class Fume {
 public:
  Fume(std::shared_ptr<Synchronizer> synchronizer, int pool_size, SdkOptions& options)
      : queue_(synchronizer, pool_size) {
    Aws::Client::ClientConfiguration client_config_;
    client_config_.region = Aws::Utils::StringUtils::to_string(options.region);
    client_config_.endpointOverride =
        Aws::Utils::StringUtils::to_string(options.endpoint_override);
    if (options.scheme == "http") {
      client_config_.scheme = Aws::Http::Scheme::HTTP;
    } else if (options.scheme == "https") {
      client_config_.scheme = Aws::Http::Scheme::HTTPS;
    } else {
      throw "Invalid Lambda connection scheme '", options.scheme, "'";
    }
    client_config_.retryStrategy = std::make_shared<ConnectRetryStrategy>();
    client_.reset(new Aws::Lambda::LambdaClient(client_config_));
  }

  void Invoke(const char* function_name) {
    queue_.PushRequest([function_name, this]() {
      Aws::Lambda::Model::InvokeRequest req;
      req.SetFunctionName(function_name);
      req.SetInvocationType(Aws::Lambda::Model::InvocationType::Event);
      auto object_outcome = client_->Invoke(req);
      if (!object_outcome.IsSuccess()) {
        exit(1);
      }

      return InvokeResponse{};
    });
  }

  std::vector<Result<InvokeResponse>> ProcessResponses() { return queue_.PopResponses(); }

 private:
  std::shared_ptr<Aws::Lambda::LambdaClient> client_;
  AsyncQueue<InvokeResponse> queue_;
};

void execute() {
  SdkOptions options;
  options.region = "eu-west-1";
  if (IS_LOCAL) {
    options.endpoint_override = "???:???";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  auto synchronizer = std::make_shared<Synchronizer>();
  Fume fume{synchronizer, NB_PARALLEL, options};
  for (int i = 0; i < NB_INVOKE; i++) {
    fume.Invoke(BEE_FUNCTION_NAME);
  }
  int invokes_completed = 0;
  int invokes_successful = 0;
  while (invokes_completed < NB_INVOKE) {
    synchronizer->wait();
    auto results = fume.ProcessResponses();
    invokes_completed += results.size();
    for (auto& res : results) {
      if (res.ok()) {
        invokes_successful++;
      }
    }
  }
  std::cout << "invokes_scheduled=" << NB_INVOKE << std::endl;
  std::cout << "invokes_successful=" << invokes_successful << std::endl;
}

int main() {
  InitializeAwsSdk(AwsSdkLogLevel::Off);

  execute();

  FinalizeAwsSdk();
}