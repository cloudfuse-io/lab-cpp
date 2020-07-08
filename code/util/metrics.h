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

#include <arrow/status.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace util {

using namespace arrow;

struct MetricEvent {
  std::chrono::_V2::system_clock::time_point time;
  std::thread::id thread_id;
  std::string type;
};

class MetricsManager {
 public:
  MetricsManager();
  ~MetricsManager();
  void Print() const;
  Status NewEvent(std::string type);
  void Reset();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace util