// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "downloader.h"

#include <arrow/result.h>
#include <aws/core/client/RetryStrategy.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>

#include <cassert>

#include "toolbox.h"

namespace {
template <typename Error>
inline bool IsConnectError(const Aws::Client::AWSError<Error>& error) {
  if (error.ShouldRetry()) {
    return true;
  }
  // Sometimes Minio may fail with a 503 error
  // (exception name: XMinioServerNotInitialized,
  //  message: "Server not initialized, please try again")
  if (error.GetExceptionName() == "XMinioServerNotInitialized") {
    return true;
  }
  return false;
}

class ConnectRetryStrategy : public Aws::Client::RetryStrategy {
 public:
  static const int32_t kDefaultRetryInterval = 200;     /* milliseconds */
  static const int32_t kDefaultMaxRetryDuration = 6000; /* milliseconds */

  explicit ConnectRetryStrategy(int32_t retry_interval = kDefaultRetryInterval,
                                int32_t max_retry_duration = kDefaultMaxRetryDuration)
      : retry_interval_(retry_interval), max_retry_duration_(max_retry_duration) {}

  bool ShouldRetry(const Aws::Client::AWSError<Aws::Client::CoreErrors>& error,
                   long attempted_retries) const override {  // NOLINT
    if (!IsConnectError(error)) {
      // Not a connect error, don't retry
      return false;
    }
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

std::string FormatRange(int64_t start, int64_t length) {
  // Format a HTTP range header value
  std::stringstream ss;
  ss << "bytes=" << start << "-" << start + length - 1;
  return ss.str();
}

class StringViewStream : Aws::Utils::Stream::PreallocatedStreamBuf, public std::iostream {
 public:
  StringViewStream(const void* data, int64_t nbytes)
      : Aws::Utils::Stream::PreallocatedStreamBuf(
            reinterpret_cast<unsigned char*>(const_cast<void*>(data)),
            static_cast<size_t>(nbytes)),
        std::iostream(this) {}
};

Aws::IOStreamFactory AwsWriteableStreamFactory(void* data, int64_t nbytes) {
  return [data, nbytes]() {
    // Aws::New to avoid Valgrind "Mismatched free"
    return Aws::New<StringViewStream>("downloader", data, nbytes);
  };
}

Status S3ErrorToStatus(const Aws::Client::AWSError<Aws::S3::S3Errors>& error) {
  return Status::IOError("AWS Error [code ", static_cast<int>(error.GetErrorType()),
                         "]: ", error.GetMessage());
}

Status GetObjectRange(std::shared_ptr<Aws::S3::S3Client> client, const S3Path& path,
                      int64_t start, int64_t length, void* out,
                      std::shared_ptr<util::MetricsManager> metrics_manager) {
  Aws::S3::Model::GetObjectRequest req;
  req.SetBucket(Aws::Utils::StringUtils::to_string(path.bucket));
  req.SetKey(Aws::Utils::StringUtils::to_string(path.key));
  req.SetRange(Aws::Utils::StringUtils::to_string(FormatRange(start, length)));
  req.SetResponseStreamFactory(AwsWriteableStreamFactory(out, length));
  auto object_outcome = client->GetObject(req);
  if (!object_outcome.IsSuccess()) {
    return S3ErrorToStatus(object_outcome.GetError());
  }
  auto object_result = std::move(object_outcome).GetResultWithOwnership();
  auto& stream = object_result.GetBody();
  stream.ignore(length);
  if (stream.gcount() != length) {
    return Status::IOError("Read ", stream.gcount(), " bytes instead of ", length);
  }
  return Status::OK();
}
}  // namespace

Downloader::Downloader(std::shared_ptr<Synchronizer> synchronizer, int pool_size,
                       std::shared_ptr<util::MetricsManager> metrics,
                       const S3Options& options)
    : queue_(synchronizer, pool_size),
      metrics_manager_(metrics),
      pool_size_(pool_size),
      synchronizer_(synchronizer) {
  Aws::Client::ClientConfiguration client_config_;
  client_config_.region = Aws::Utils::StringUtils::to_string(options.region);
  client_config_.endpointOverride =
      Aws::Utils::StringUtils::to_string(options.endpoint_override);
  if (options.scheme == "http") {
    client_config_.scheme = Aws::Http::Scheme::HTTP;
  } else if (options.scheme == "https") {
    client_config_.scheme = Aws::Http::Scheme::HTTPS;
  } else {
    throw "Invalid S3 connection scheme '", options.scheme, "'";
  }
  client_config_.retryStrategy = std::make_shared<ConnectRetryStrategy>();
  bool use_virtual_addressing = options.endpoint_override.empty();
  client_.reset(new Aws::S3::S3Client(
      client_config_, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
      use_virtual_addressing));
}

void Downloader::InitConnections(std::string bucket, int max_init_count) {
  assert(max_init_count <= pool_size_);
  {
    const std::lock_guard<std::mutex> lock(init_interruption_mutex_);
    init_counter_ = 0;
  }
  auto start = util::time::now();
  for (int i = 0; i < max_init_count; i++) {
    queue_.PushRequest([this, bucket, max_init_count,
                        start]() -> Result<DownloadResponse> {
      {
        const std::lock_guard<std::mutex> lock(init_interruption_mutex_);
        if (init_counter_ >= max_init_count) {
          auto time_since_init = util::get_duration_ms(start, util::time::now());
          metrics_manager_->NewInitConnection(time_since_init, 0, 0);
          return STATUS_ABORTED;
        }
      }
      // use Delete requests because they are free hahaha...
      Aws::S3::Model::DeleteObjectRequest req;
      req.SetBucket(Aws::Utils::StringUtils::to_string(bucket));
      req.SetKey(Aws::Utils::StringUtils::to_string("fakekey"));
      // we block before releasing the connection to allow up to
      // max_init_count initializations
      util::time::time_point blocking_start_time;
      util::time::time_point blocking_end_time;
      req.SetDataReceivedEventHandler([this, max_init_count, &blocking_start_time,
                                       &blocking_end_time](const Aws::Http::HttpRequest*,
                                                           const Aws::Http::HttpResponse*,
                                                           long long) -> void {
        if (blocking_start_time.time_since_epoch().count()) {
          // the callback was already called for this init
          return;
        }
        blocking_start_time = util::time::now();
        std::unique_lock<std::mutex> lock(init_interruption_mutex_);
        init_counter_++;
        if (init_counter_ < max_init_count) {
          init_interruption_cv_.wait(
              lock, [this, max_init_count]() { return init_counter_ >= max_init_count; });
        } else {
          lock.unlock();
          init_interruption_cv_.notify_all();
        }
        blocking_end_time = util::time::now();
      });
      client_->DeleteObject(req);
      // if resolution_time_ms << 0 it means that the callback was never called
      metrics_manager_->NewInitConnection(
          util::get_duration_ms(start, util::time::now()),
          util::get_duration_ms(start, blocking_start_time),
          util::get_duration_ms(blocking_start_time, blocking_end_time));

      return DownloadResponse{};
    });
  }
}

void Downloader::ScheduleDownload(DownloadRequest request) {
  {
    const std::lock_guard<std::mutex> lock(init_interruption_mutex_);
    init_counter_ = pool_size_;
  }
  init_interruption_cv_.notify_all();
  queue_.PushRequest([request, this]() -> Result<DownloadResponse> {
    int64_t nbytes = request.range_end - request.range_start + 1;
    ARROW_ASSIGN_OR_RAISE(auto buf, arrow::AllocateResizableBuffer(nbytes));
    auto start = util::time::now();
    this->metrics_manager_->NewEvent("get_obj_start");
    RETURN_NOT_OK(GetObjectRange(this->client_, request.path, request.range_start, nbytes,
                                 buf->mutable_data(), this->metrics_manager_));
    this->metrics_manager_->NewDownload(util::get_duration_ms(start, util::time::now()),
                                        nbytes);
    this->metrics_manager_->NewEvent("get_obj_end");

    return DownloadResponse{request, std::shared_ptr<arrow::Buffer>(std::move(buf))};
  });
}

std::vector<Result<DownloadResponse>> Downloader::ProcessResponses() {
  return queue_.PopResponses();
}