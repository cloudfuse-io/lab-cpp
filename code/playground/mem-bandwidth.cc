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

#include <thread>

#include "bootstrap.h"
#include "logger.h"

static const bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);
static util::Logger LOGGER = util::Logger(IS_LOCAL);

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req) {
  std::vector<int64_t*> arrays;
  for (int run = 0; run < 5; run++) {
    constexpr int array_size = 8 * 1024 * 1024;

    auto alloc_start_time = std::chrono::high_resolution_clock::now();
    auto values = new int64_t[array_size];
    for (int i = 0; i < array_size; i++) {
      values[i] = i;  // pseudo random
    }
    auto alloc_end_time = std::chrono::high_resolution_clock::now();

    arrays.push_back(values);
    auto agg_start_time = std::chrono::high_resolution_clock::now();
    int64_t sum = 0;
    for (int i = 0; i < array_size; i++) {
      sum += values[i];
    }
    auto agg_end_time = std::chrono::high_resolution_clock::now();

    auto agg_duration = util::get_duration_micro(agg_start_time, agg_end_time);
    auto alloc_duration = util::get_duration_micro(alloc_start_time, alloc_end_time);
    constexpr double GIGA = 1024. * 1024. * 1024.;
    constexpr double array_bytes = array_size * sizeof(int64_t);
    auto entry = LOGGER.NewEntry("mem_bandwidth");
    entry.FloatField("agg_GBpS", array_bytes / agg_duration * 1000. * 1000. / GIGA);
    entry.IntField("agg_ms", agg_duration / 1000);
    entry.FloatField("alloc_GBpS", array_bytes / alloc_duration * 1000. * 1000. / GIGA);
    entry.IntField("alloc_ms", alloc_duration / 1000);
    entry.IntField("computed_sum", sum);
    entry.FloatField("size_GB", array_bytes / GIGA);
    entry.Log();
  }
  for (auto array : arrays) {
    delete[] array;
  }
  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

int main() { return bootstrap(my_handler); }