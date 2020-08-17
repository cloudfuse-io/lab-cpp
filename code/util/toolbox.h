// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements. See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership. The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <stdlib.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <string>

namespace Buzz {

using time = std::chrono::high_resolution_clock;

namespace util {

inline int64_t get_duration_ms(time::time_point start, time::time_point end) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

inline int64_t get_duration_micro(time::time_point start, time::time_point end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

inline int64_t getenv_int(const char* name, int64_t def) {
  auto raw_var = ::getenv(name);
  if (raw_var == nullptr) {
    return def;
  }
  return std::stoi(raw_var);
}

inline int64_t getenv_bool(const char* name, bool def) {
  auto raw_var = ::getenv(name);
  if (raw_var == nullptr) {
    return def;
  }
  return strcmp(raw_var, "true") == 0;
}

inline const char* getenv(const char* name, const char* def) {
  auto raw_var = ::getenv(name);
  if (raw_var == nullptr) {
    return def;
  }
  return raw_var;
}

/// generate a "random" char from the current time low bits
inline char random_alphanum() {
  constexpr char charset[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  constexpr size_t max_index = (sizeof(charset) - 1);
  return charset[time::now().time_since_epoch().count() % max_index];
}

}  // namespace util

}  // namespace Buzz