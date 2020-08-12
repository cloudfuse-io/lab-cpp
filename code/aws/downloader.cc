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

#include <aws/core/client/DefaultRetryStrategy.h>
#include <aws/core/client/RetryStrategy.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <result.h>
#include <toolbox.h>

#include <cassert>
#include <string>

namespace Buzz {

namespace {
std::string FormatRange(std::optional<int64_t> start, int64_t end) {
  // Format a HTTP range header value
  std::stringstream ss;
  ss << "bytes=";
  if (start.has_value()) {
    ss << start.value();
  }
  ss << "-";
  ss << end;
  return ss.str();
}

/// get file size from range string
Result<int64_t> ParseRange(std::string response_range) {
  auto size_split = response_range.find("/");
  if (size_split == std::string::npos) {
    return Status::IOError("Unexpected content range: ", response_range);
  }
  return std::stoll(response_range.substr(size_split + 1));
}

int64_t CalculateLength(std::optional<int64_t> start, int64_t end) {
  if (!start.has_value()) {
    return end;
  } else {
    return end - start.value() + 1;
  }
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

struct ObjectRangeResult {
  std::shared_ptr<arrow::Buffer> raw_data;
  int64_t file_size;
};

Result<ObjectRangeResult> GetObjectRange(
    std::shared_ptr<Aws::S3::S3Client> client, const S3Path& path,
    std::optional<int64_t> start, int64_t end,
    std::shared_ptr<MetricsManager> metrics_manager) {
  auto nbytes = CalculateLength(start, end);
  ARROW_ASSIGN_OR_RAISE(auto buf, arrow::AllocateResizableBuffer(nbytes));
  Aws::S3::Model::GetObjectRequest req;
  req.SetBucket(path.bucket);
  req.SetKey(path.key);
  req.SetRange(FormatRange(start, end));
  req.SetResponseStreamFactory(AwsWriteableStreamFactory(buf->mutable_data(), nbytes));
  auto start_time = time::now();
  auto object_outcome = client->GetObject(req);
  metrics_manager->NewDownload(util::get_duration_ms(start_time, time::now()), nbytes);
  if (!object_outcome.IsSuccess()) {
    return S3ErrorToStatus(object_outcome.GetError());
  }
  auto object_result = std::move(object_outcome).GetResultWithOwnership();
  // extract from headers
  ARROW_ASSIGN_OR_RAISE(auto file_size, ParseRange(object_result.GetContentRange()));
  // extract body
  auto& stream = object_result.GetBody();
  stream.ignore(nbytes);
  if (stream.gcount() != nbytes) {
    return Status::IOError("Read ", stream.gcount(), " bytes instead of ", nbytes);
  }
  return ObjectRangeResult{std::move(buf), file_size};
}
}  // namespace

Aws::Client::ClientConfiguration common_config(const SdkOptions& options) {
  Aws::Client::ClientConfiguration conf;
  conf.region = options.region;
  conf.endpointOverride = options.endpoint_override;
  if (options.scheme == "http") {
    conf.scheme = Aws::Http::Scheme::HTTP;
  } else if (options.scheme == "https") {
    conf.scheme = Aws::Http::Scheme::HTTPS;
  } else {
    throw "Invalid S3 connection scheme '", options.scheme, "'";
  }
  return conf;
}

Downloader::Downloader(std::shared_ptr<Synchronizer> synchronizer, int pool_size,
                       std::shared_ptr<MetricsManager> metrics, const SdkOptions& options)
    : queue_(synchronizer, pool_size),
      metrics_manager_(metrics),
      pool_size_(pool_size),
      synchronizer_(synchronizer) {
  bool use_virtual_addressing = options.endpoint_override.empty();
  // DL CLIENT FOR DOWNLOADS
  Aws::Client::ClientConfiguration dl_config_ = common_config(options);
  dl_config_.retryStrategy = std::make_shared<Aws::Client::DefaultRetryStrategy>(3);
  dl_client_.reset(new Aws::S3::S3Client(
      dl_config_, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
      use_virtual_addressing));
  // INIT CLIENT FOR INIT CONNECTION OPS, no retry shorter timeout
  Aws::Client::ClientConfiguration init_config_ = common_config(options);
  init_config_.retryStrategy = std::make_shared<Aws::Client::DefaultRetryStrategy>(0);
  init_config_.httpRequestTimeoutMs = 500;
  init_config_.connectTimeoutMs = 500;
  init_client_.reset(new Aws::S3::S3Client(
      init_config_, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
      use_virtual_addressing));
}

void Downloader::InitConnections(std::string bucket, int max_init_count) {
  assert(max_init_count <= pool_size_);
  {
    const std::lock_guard<std::mutex> lock(init_interruption_mutex_);
    init_counter_ = 0;
  }
  auto start = time::now();
  for (int i = 0; i < max_init_count; i++) {
    queue_.PushRequest([this, bucket, max_init_count, start,
                        i]() -> Result<DownloadResponse> {
      {
        const std::lock_guard<std::mutex> lock(init_interruption_mutex_);
        if (init_counter_ >= max_init_count) {
          auto time_since_init = util::get_duration_ms(start, time::now());
          metrics_manager_->NewInitConnection("ABORTED", time_since_init, 0, 0);
          return STATUS_ABORTED;
        }
      }
      // use Delete requests because they are free hahaha...
      Aws::S3::Model::DeleteObjectRequest req;
      req.SetBucket(bucket);
      // TODO how should we prefix to avoid slow down ?
      std::string randomized_key;
      randomized_key.push_back(util::random_alphanum());
      randomized_key += "/buzzfakekey";
      std::cout << "randomized_key:" << randomized_key << std::endl;
      req.SetKey(randomized_key);
      // we block before releasing the connection to allow up to
      // max_init_count initializations
      time::time_point blocking_start_time;
      time::time_point blocking_end_time;
      req.SetDataReceivedEventHandler([this, max_init_count, &blocking_start_time,
                                       &blocking_end_time](const Aws::Http::HttpRequest*,
                                                           const Aws::Http::HttpResponse*,
                                                           long long) -> void {
        if (blocking_start_time.time_since_epoch().count()) {
          // the callback was already called for this init
          return;
        }
        blocking_start_time = time::now();
        std::unique_lock<std::mutex> lock(init_interruption_mutex_);
        init_counter_++;
        if (init_counter_ < max_init_count) {
          init_interruption_cv_.wait(
              lock, [this, max_init_count]() { return init_counter_ >= max_init_count; });
        } else {
          lock.unlock();
          init_interruption_cv_.notify_all();
        }
        blocking_end_time = time::now();
      });
      auto outcome = init_client_->DeleteObject(req);
      std::string result;
      if (outcome.IsSuccess()) {
        result = "OK";
      } else {
        result = outcome.GetError().GetMessage();
      }
      if (!blocking_start_time.time_since_epoch().count() &&
          !blocking_end_time.time_since_epoch().count()) {
        // sync_callback never called, likely because of timeout
        blocking_start_time = blocking_end_time = time::now();
        std::unique_lock<std::mutex> lock(init_interruption_mutex_);
        init_counter_++;
        if (init_counter_ == max_init_count) {
          lock.unlock();
          init_interruption_cv_.notify_all();
        }
      }
      // if resolution_time_ms << 0 it means that the callback was never called
      metrics_manager_->NewInitConnection(
          std::move(result), util::get_duration_ms(start, time::now()),
          util::get_duration_ms(start, blocking_start_time),
          util::get_duration_ms(blocking_start_time, blocking_end_time));

      return DownloadResponse{};
    });
  }
}

void Downloader::ScheduleDownload(DownloadRequest request) {
  // cancel all pending inits to replace them with real work
  {
    const std::lock_guard<std::mutex> lock(init_interruption_mutex_);
    init_counter_ = pool_size_;
  }
  init_interruption_cv_.notify_all();
  // queue the request
  queue_.PushRequest([request, this]() -> Result<DownloadResponse> {
    metrics_manager_->NewEvent("get_obj_start");
    ARROW_ASSIGN_OR_RAISE(auto result,
                          GetObjectRange(dl_client_, request.path, request.range_start,
                                         request.range_end, metrics_manager_));
    metrics_manager_->NewEvent("get_obj_end");
    return DownloadResponse{request, result.raw_data, result.file_size};
  });
}

std::vector<Result<DownloadResponse>> Downloader::ProcessResponses() {
  return queue_.PopResponses();
}

}  // namespace Buzz
