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

#include <arrow/api.h>
#include <result.h>

#include <string>

// #define ACTIVATE_RUNWAY_ALLOCATOR
// #define ACTIVATE_POOL_ALLOCATOR
// #define ACTIVATE_ALLOCATION_LINKING
// #define ACTIVATE_ALLOCATION_PRINTING

namespace Buzz {

inline constexpr int64_t HUGE_ALLOC_THRESHOLD_BYTES = 1024 * 1024;
inline constexpr int64_t SMALL_ALLOC_THRESHOLD_BYTES = 256 * 1024;
inline constexpr int64_t HUGE_ALLOC_RUNWAY_SIZE_BYTES = 1024 * 1024 * 1024;

/// Derived class for memory allocation.
///
/// Optimizes large allocations an re-allocations
/// Forwards small allocations to inner allocator
class ARROW_EXPORT CustomMemoryPool : public arrow::MemoryPool {
 public:
  explicit CustomMemoryPool(arrow::MemoryPool* pool);
  ~CustomMemoryPool() override;

  Status Allocate(int64_t size, uint8_t** out) override;
  Status Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) override;

  void Free(uint8_t* buffer, int64_t size) override;

  int64_t bytes_allocated() const override;

  int64_t max_memory() const override;

  int64_t copied_bytes() const;

  std::string backend_name() const override;

 private:
  class CustomMemoryPoolImpl;
  std::unique_ptr<CustomMemoryPoolImpl> impl_;
};

}  // namespace Buzz
