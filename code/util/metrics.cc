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

#include "metrics.h"

#include <algorithm>
#include <iostream>
#include <map>

#include "toolbox.h"

namespace util {

using namespace arrow;

class MetricsManager::Impl {
 public:
  mutable std::mutex metrics_mutex_;
  std::vector<MetricEvent> events_;
  std::vector<int64_t> reads_;
  std::chrono::_V2::system_clock::time_point ref_time =
      std::chrono::high_resolution_clock::now();

  void PrintEvents() const {
    // event timing stats
    std::lock_guard<std::mutex> guard(metrics_mutex_);
    std::map<std::thread::id, std::vector<MetricEvent>> thread_map;
    for (const auto& event : events_) {
      thread_map[event.thread_id].push_back(event);
    }

    std::multimap<int64_t, std::vector<MetricEvent>> sorted_threads;
    for (const auto& thread : thread_map) {
      auto metric_events = thread.second;
      std::sort(metric_events.begin(), metric_events.end(),
                [](MetricEvent const& a, MetricEvent const& b) -> bool {
                  return a.time < b.time;
                });
      auto first_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            metric_events[0].time - ref_time)
                            .count();
      sorted_threads.insert({first_time, metric_events});
    }

    for (const auto& thread : sorted_threads) {
      std::cout << thread.second[0].thread_id;
      auto previous_time = ref_time;
      for (const auto& event : thread.second) {
        std::cout << ",";
        std::cout << ::util::get_duration_ms(previous_time, event.time);
        previous_time = event.time;
      }
      std::cout << std::endl;
    }
  }

  Status NewEvent(std::string type) {
    MetricEvent event{
        std::chrono::high_resolution_clock::now(),
        std::this_thread::get_id(),
        type,
    };
    std::lock_guard<std::mutex> guard(metrics_mutex_);
    events_.push_back(event);
    return Status::OK();
  }
};

MetricsManager::MetricsManager() : impl_(new Impl{}) {}
MetricsManager::~MetricsManager() {}

void MetricsManager::Print() const { impl_->PrintEvents(); }

Status MetricsManager::NewEvent(std::string type) { return impl_->NewEvent(type); }

}  // namespace util