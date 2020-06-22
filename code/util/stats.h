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

#include <arrow/status.h>
#include <parquet/api/schema.h>

#include <string_view>
#include <unordered_map>

namespace util {
using namespace arrow;

template <typename Key>
struct HashingTraits {
  using Hash = std::hash<Key>;
  using Equal = std::equal_to<Key>;
};

template <>
struct HashingTraits<parquet::ByteArray> {
  using Key = parquet::ByteArray;
  using Hash = struct {
    std::size_t operator()(Key const& s) const noexcept {
      auto ptr_reinterp = reinterpret_cast<const char*>(s.ptr);
      return std::hash<std::string_view>{}(std::string_view(ptr_reinterp, s.len));
    }
  };
  using Equal = std::equal_to<Key>;
};

template <typename Key>
class CountStat {
 public:
  using Hash = typename HashingTraits<Key>::Hash;
  using Equal = typename HashingTraits<Key>::Equal;

  CountStat();
  void Print() const;
  Status Add(Key* items, int64_t len);

 private:
  std::unordered_map<Key, int64_t, Hash, Equal> counts_;
};
}  // namespace util