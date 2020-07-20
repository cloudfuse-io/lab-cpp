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

#include <aws/lambda-runtime/runtime.h>

#include <cmath>
#include <thread>

#include "bootstrap.h"
#include "logger.h"

constexpr int ARRAY_SIZE = 8 * 1024 * 1024;
constexpr double GIGA = 1024. * 1024. * 1024.;
constexpr double ARRAY_BYTES = ARRAY_SIZE * sizeof(int64_t);
static const int64_t CPU_FOR_SECOND_THREAD = util::getenv_int("CPU_FOR_SECOND_THREAD", 1);
static bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req) {
  std::vector<int64_t*> arrays;
  for (int run = 0; run < 5; run++) {
    // alloc array
    auto alloc_start_time = std::chrono::high_resolution_clock::now();
    int64_t* values;
    posix_memalign(reinterpret_cast<void**>(&values), 64,
                   static_cast<size_t>(ARRAY_BYTES));
    for (int i = 0; i < ARRAY_SIZE; i++) {
      values[i] = i;
    }
    auto alloc_end_time = std::chrono::high_resolution_clock::now();
    auto alloc_duration = util::get_duration_micro(alloc_start_time, alloc_end_time);
    arrays.push_back(values);
    std::vector<std::thread> threads(2);
    for (unsigned thread_nb = 0; thread_nb < 2; ++thread_nb) {
      // setup thread
      threads[thread_nb] = std::thread([values, thread_nb] {
        auto agg_start_time = std::chrono::high_resolution_clock::now();
        int64_t sum = 0;
        for (int i = 0; i < ARRAY_SIZE; i++) {
          sum += std::sin(values[i]);
        }
        auto agg_end_time = std::chrono::high_resolution_clock::now();
        auto agg_duration = util::get_duration_micro(agg_start_time, agg_end_time);

        auto entry = Buzz::logger::NewEntry("mem_bandwidth");
        entry.FloatField("agg_GBpS", ARRAY_BYTES / agg_duration * 1000. * 1000. / GIGA);
        entry.IntField("agg_ms", agg_duration / 1000);
        entry.IntField("computed_sum", sum);
        // entry.FloatField("size_GB", ARRAY_BYTES / GIGA);
        entry.IntField("core", sched_getcpu());
        entry.Log();
      });

      // set thread affinity
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(thread_nb * CPU_FOR_SECOND_THREAD, &cpuset);
      int rc = pthread_setaffinity_np(threads[thread_nb].native_handle(),
                                      sizeof(cpu_set_t), &cpuset);
      if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
      }
    }
    // wait for all to complete
    for (auto& t : threads) {
      t.join();
    }
  }
  // clean up
  for (auto array : arrays) {
    delete[] array;
  }
  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

int main() { bootstrap(my_handler); }