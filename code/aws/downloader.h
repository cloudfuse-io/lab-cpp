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

#pragma once

#include <arrow/buffer.h>
#include <aws/s3/S3Client.h>
#include <result.h>

#include <condition_variable>
#include <string>
#include <vector>

#include "async_queue.h"
#include "metrics.h"
#include "sdk-init.h"

namespace Buzz {

#if !defined(BUZZ_STATUS_ABORT)
#define BUZZ_STATUS_ABORT 1
const Status STATUS_ABORTED(StatusCode::UnknownError, "query_aborted");
#endif

struct S3Path {
  std::string bucket;
  std::string key;

  std::string ToString() const { return bucket + "/" + key; }
};

/// if range_start==range_end==0 this is an init request
/// else follow https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.35
/// bytes=range_start-range_end
struct DownloadRequest {
  std::optional<int64_t> range_start;
  int64_t range_end;
  S3Path path;
};

struct DownloadResponse {
  DownloadRequest request;
  std::shared_ptr<arrow::Buffer> raw_data;
  int64_t file_size;
};

class Downloader {
 public:
  /// The Synchronizer allows the downloader to notify the dispatcher when a new
  /// download is ready
  Downloader(std::shared_ptr<Synchronizer> synchronizer, int pool_size,
             std::shared_ptr<MetricsManager> metrics, const SdkOptions& options);

  /// max_init_count should be <= than pool_size
  /// TODO: if called again before previous init complete, behaviour is undefined
  void InitConnections(std::string bucket, int max_init_count);

  /// Add a new download to the threadpool queue
  void ScheduleDownload(DownloadRequest request);

  /// Get all the responses in the response queue
  std::vector<Result<DownloadResponse>> ProcessResponses();

 private:
  int pool_size_;
  std::shared_ptr<Synchronizer> synchronizer_;
  AsyncQueue<DownloadResponse> queue_;
  std::shared_ptr<Aws::S3::S3Client> dl_client_;
  std::shared_ptr<Aws::S3::S3Client> init_client_;
  std::shared_ptr<MetricsManager> metrics_manager_;
  int init_counter_;
  std::condition_variable init_interruption_cv_;
  std::mutex init_interruption_mutex_;
};

}  // namespace Buzz
