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
#include <string>

namespace util {
inline int64_t get_duration_ms(std::chrono::_V2::system_clock::time_point start,
                               std::chrono::_V2::system_clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

inline int64_t get_duration_micro(std::chrono::_V2::system_clock::time_point start,
                                  std::chrono::_V2::system_clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

inline int64_t getenv_int(const char* name, int64_t def) {
  auto raw_var = getenv(name);
  if (raw_var == nullptr) {
    return def;
  }
  return std::stoi(raw_var);
}
}  // namespace util