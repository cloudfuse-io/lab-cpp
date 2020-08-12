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

#include "logger.h"
#include "toolbox.h"

namespace Buzz {

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
  std::string result;
  int64_t total_duration_ms;
  int64_t resolution_time_ms;
  int64_t blocking_time_ms;
};

struct PhaseDurations {
  std::vector<int64_t> durations;
  std::optional<time::time_point> started;
};
}  // namespace

class MetricsManager::Impl {
 public:
  concurrent_vector<MetricEvent> events_;
  concurrent_vector<Download> downloads_;
  concurrent_vector<InitConnection> init_connections_;
  std::map<std::string, PhaseDurations> phase_durations_map_;
  time::time_point ref_time_ = time::now();

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
      auto first_time = util::get_duration_ms(ref_time_, metric_events[0].timestamp);
      sorted_threads.insert({first_time, metric_events});
    }

    for (const auto& thread : sorted_threads) {
      std::cout << thread.second[0].thread_id;
      auto previous_time = ref_time_;
      for (const auto& event : thread.second) {
        std::cout << ",";
        std::cout << util::get_duration_ms(previous_time, event.timestamp);
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
      auto entry = logger::NewEntry("downloads", event.timestamp);
      entry.IntField("duration_ms", event.duration_ms);
      entry.IntField("size_B", event.size);
      entry.FloatField("speed_MBpS",
                       (event.size / 1000000.) / (event.duration_ms / 1000.));
      entry.Log();
    }
  }

  void PrintPhases() const {
    std::vector<std::pair<std::string, int64_t>> total_durations;
    for (const auto& phase_durations_kv : phase_durations_map_) {
      // compute total duration of the phase
      int64_t total_dur = 0;
      for (auto dur : phase_durations_kv.second.durations) {
        total_dur += dur;
      }
      total_durations.emplace_back(phase_durations_kv.first, total_dur);
      // print the detail of the phase durations
      std::cout << phase_durations_kv.first << ":";
      for (auto dur : phase_durations_kv.second.durations) {
        std::cout << dur << ",";
      }
      std::cout << std::endl;
    }
    auto entry = logger::NewEntry("phase_durations");
    for (auto& duration : total_durations) {
      entry.IntField(duration.first.data(), duration.second);
    }
    entry.Log();
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
      auto entry = logger::NewEntry("init_connection", event.timestamp);
      entry.StrField("result", event.result.data());
      entry.IntField("total_duration_ms", event.total_duration_ms);
      entry.IntField("blocking_time_ms", event.blocking_time_ms);
      entry.IntField("resolution_time_ms", event.resolution_time_ms);
      entry.Log();
    }
  }

  void NewInitConnection(std::string result, int64_t total_duration_ms,
                         int64_t resolution_time_ms, int64_t blocking_time_ms) {
    InitConnection init_connection{
        time::now(),       std::this_thread::get_id(), std::move(result),
        total_duration_ms, resolution_time_ms,         blocking_time_ms,
    };
    init_connections_.push_back(init_connection);
  }

  void EnterPhase(std::string name) {
    auto item = phase_durations_map_.find(name);
    if (item == phase_durations_map_.end()) {
      phase_durations_map_.emplace(name, PhaseDurations{{}, time::now()}).first;
    } else if (item->second.started.has_value()) {
      // TODO return error
      std::cout << "Phase already entered" << std::endl;
    } else {
      item->second.started = time::now();
    }
  }

  void ExitPhase(std::string name) {
    auto item = phase_durations_map_.find(name);
    if (item == phase_durations_map_.end()) {
      // TODO return error
      std::cout << "Phase never entered" << std::endl;
    } else if (item->second.started.has_value()) {
      item->second.durations.push_back(
          util::get_duration_ms(item->second.started.value(), time::now()));
      item->second.started = {};
    } else {
      // TODO return error
      std::cout << "Phase already exited" << std::endl;
    }
  }

  void Reset() {
    ref_time_ = time::now();
    events_.clear();
    downloads_.clear();
    init_connections_.clear();
  }
};

MetricsManager::MetricsManager() : impl_(new Impl{}) {}
MetricsManager::~MetricsManager() {}

void MetricsManager::Print() const {
  impl_->PrintEvents();
  impl_->PrintInitConnections();
  // impl_->PrintDownloads();
  impl_->PrintPhases();
}

void MetricsManager::NewEvent(std::string type) { impl_->NewEvent(type); }

void MetricsManager::NewDownload(int64_t duration_ms, int64_t size) {
  impl_->NewDownload(duration_ms, size);
}
void MetricsManager::NewInitConnection(std::string result, int64_t total_duration_ms,
                                       int64_t resolution_time_ms,
                                       int64_t blocking_time_ms) {
  impl_->NewInitConnection(std::move(result), total_duration_ms, resolution_time_ms,
                           blocking_time_ms);
}

void MetricsManager::EnterPhase(std::string name) { impl_->EnterPhase(name); }

void MetricsManager::ExitPhase(std::string name) { impl_->ExitPhase(name); }

void MetricsManager::Reset() { impl_->Reset(); }

}  // namespace Buzz