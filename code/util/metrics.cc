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

namespace {

/// a simple mutex synched append only vect
template <class T>
class concurrent_vector {
 public:
  void push_back(T elem) {
    std::lock_guard<std::mutex> guard(mutex_);
    vect_.push_back(elem);
  }

  std::vector<T> clone() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<T> copy_vect;
    copy_vect.reserve(vect_.size());
    for (auto& item : vect_) {
      copy_vect.push_back(item);
    }
    return copy_vect;
  }

  void clear() {
    std::lock_guard<std::mutex> guard(mutex_);
    vect_.clear();
  }

 private:
  mutable std::mutex mutex_;
  std::vector<T> vect_;
};

struct MetricEvent {
  time::time_point timestamp;
  std::thread::id thread_id;
  std::string type;
};

struct Download {
  time::time_point timestamp;
  std::thread::id thread_id;
  int64_t duration_ms;
  int64_t size;
};

struct InitConnection {
  time::time_point timestamp;
  std::thread::id thread_id;
  int64_t total_duration_ms;
  int64_t resolution_time_ms;
  int64_t blocking_time_ms;
};
}  // namespace

class MetricsManager::Impl {
 public:
  util::Logger logger_;
  concurrent_vector<MetricEvent> events_;
  concurrent_vector<Download> downloads_;
  concurrent_vector<InitConnection> init_connections_;
  time::time_point ref_time_ = time::now();

  Impl(util::Logger logger) : logger_(logger) {}

  void PrintEvents() const {
    // event timing stats
    std::map<std::thread::id, std::vector<MetricEvent>> thread_map;
    for (const auto& event : events_.clone()) {
      thread_map[event.thread_id].push_back(event);
    }

    std::multimap<int64_t, std::vector<MetricEvent>> sorted_threads;
    for (const auto& thread : thread_map) {
      auto metric_events = thread.second;
      std::sort(metric_events.begin(), metric_events.end(),
                [](MetricEvent const& a, MetricEvent const& b) -> bool {
                  return a.timestamp < b.timestamp;
                });
      auto first_time = ::util::get_duration_ms(ref_time_, metric_events[0].timestamp);
      sorted_threads.insert({first_time, metric_events});
    }

    for (const auto& thread : sorted_threads) {
      std::cout << thread.second[0].thread_id;
      auto previous_time = ref_time_;
      for (const auto& event : thread.second) {
        std::cout << ",";
        std::cout << ::util::get_duration_ms(previous_time, event.timestamp);
        previous_time = event.timestamp;
      }
      std::cout << std::endl;
    }
  }

  void NewEvent(std::string type) {
    MetricEvent event{
        time::now(),
        std::this_thread::get_id(),
        type,
    };
    events_.push_back(event);
  }

  void PrintDownloads() const {
    for (const auto& event : downloads_.clone()) {
      auto entry = logger_.NewEntry("downloads", event.timestamp);
      entry.IntField("duration_ms", event.duration_ms);
      entry.IntField("size_B", event.size);
      entry.FloatField("speed_MBpS",
                       (event.size / 1000000.) / (event.duration_ms / 1000.));
      entry.Log();
    }
  }

  void NewDownload(int64_t duration_ms, int64_t size) {
    Download download{
        time::now(),
        std::this_thread::get_id(),
        duration_ms,
        size,
    };
    downloads_.push_back(download);
  }

  void PrintInitConnections() const {
    for (const auto& event : init_connections_.clone()) {
      auto entry = logger_.NewEntry("init_connection", event.timestamp);
      entry.IntField("total_duration_ms", event.total_duration_ms);
      entry.IntField("blocking_time_ms", event.blocking_time_ms);
      entry.IntField("resolution_time_ms", event.resolution_time_ms);
      entry.Log();
    }
  }

  void NewInitConnection(int64_t total_duration_ms, int64_t resolution_time_ms,
                         int64_t blocking_time_ms) {
    InitConnection init_connection{
        time::now(),        std::this_thread::get_id(), total_duration_ms,
        resolution_time_ms, blocking_time_ms,
    };
    init_connections_.push_back(init_connection);
  }

  void Reset() {
    ref_time_ = time::now();
    events_.clear();
    downloads_.clear();
    init_connections_.clear();
  }
};

MetricsManager::MetricsManager(Logger logger) : impl_(new Impl{logger}) {}
MetricsManager::~MetricsManager() {}

void MetricsManager::Print() const {
  // impl_->PrintEvents();
  // impl_->PrintInitConnections();
  // impl_->PrintDownloads();
}

void MetricsManager::NewEvent(std::string type) { impl_->NewEvent(type); }

void MetricsManager::NewDownload(int64_t duration_ms, int64_t size) {
  impl_->NewDownload(duration_ms, size);
}
void MetricsManager::NewInitConnection(int64_t total_duration_ms,
                                       int64_t resolution_time_ms,
                                       int64_t blocking_time_ms) {
  impl_->NewInitConnection(total_duration_ms, resolution_time_ms, blocking_time_ms);
}

void MetricsManager::Reset() { impl_->Reset(); }

}  // namespace util