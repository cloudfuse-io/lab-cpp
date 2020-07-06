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

/// Options for the S3FileSystem implementation.
struct S3Options {
  /// AWS region to connect to (default "us-east-1")
  std::string region = "us-east-1";

  /// If non-empty, override region with a connect string such as "localhost:9000"
  // XXX perhaps instead take a URL like "http://localhost:9000"?
  std::string endpoint_override;
  /// S3 connection transport, default "https"
  std::string scheme = "https";
};

struct S3Path {
  std::string bucket;
  std::string key;
};

struct DownloadRequest {
  int64_t range_start;
  int64_t range_end;
  S3Path path;
};

struct DownloadResponse {
  DownloadRequest request;
  std::shared_ptr<arrow::Buffer> raw_data;
};

class Downloader {
 public:
  /// The Synchronizer allows the downloader to notify the dispatcher when a new
  /// download is ready
  Downloader(std::shared_ptr<Synchronizer> synchronizer, int pool_size,
             std::shared_ptr<util::MetricsManager> metrics, const S3Options& options);

  /// Add a new download to the threadpool queue
  void ScheduleDownload(DownloadRequest request);

  /// Get all the responses in the response queue
  std::vector<Result<DownloadResponse>> ProcessResponses();

 private:
  AsyncQueue<DownloadResponse> queue_;
  std::shared_ptr<Aws::S3::S3Client> client_;
  std::shared_ptr<util::MetricsManager> metrics_manager_;
};
