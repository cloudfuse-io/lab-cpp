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

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace util {

class MetricsManager {
 public:
  MetricsManager();
  ~MetricsManager();

  void Print() const;

  /// events help to reconsitute the execution timelines thread by thread
  void NewEvent(std::string type);

  /// track all individual download to detect eventual stragglers
  void NewDownload(int64_t duration_ms, int64_t size);

  /// track all ssl connection init requests to ensure that they do not slow down the
  /// downloader
  void NewInitConnection(std::string result, int64_t total_duration_ms,
                         int64_t resolution_time_ms, int64_t blocking_time_ms);

  /// Empty the manager to start tracking a new execution that maintained previous context
  void Reset();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace util