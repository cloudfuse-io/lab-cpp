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

#include "cust_memory_pool.h"

#include <sys/mman.h>
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <algorithm>  // IWYU pragma: keep
#include <boost/stacktrace.hpp>
#include <cstdlib>   // IWYU pragma: keep
#include <cstring>   // IWYU pragma: keep
#include <iostream>  // IWYU pragma: keep
#include <limits>
#include <map>
#include <memory>
#include <mutex>

#include "arrow/memory_pool.h"
#include "arrow/result.h"
#include "arrow/status.h"

#define ACTIVATE_RUNWAY_ALLOCATOR

namespace arrow {

// compute the "whole page size" equivalent of this size
size_t whole_page_size(size_t raw_size) {
  size_t page_size = 4 * 1024;

  int remainder = raw_size % page_size;
  if (remainder == 0) return raw_size;

  return raw_size + page_size - remainder;
}

class guarded_map {
 private:
  std::map<uint8_t*, int64_t> map_;
  mutable std::mutex mutex_;

 public:
  void set(uint8_t* key, int64_t value) {
    std::lock_guard<std::mutex> lk(mutex_);
    map_[key] = value;
  }

  bool erase(uint8_t* key) {
    std::lock_guard<std::mutex> lk(mutex_);
    return map_.erase(key);
  }

  bool contains(uint8_t* key) const {
    std::lock_guard<std::mutex> lk(mutex_);
    return map_.find(key) != map_.end();
  }

  int64_t sum() const {
    std::lock_guard<std::mutex> lk(mutex_);
    int64_t sum = 0;
    for (auto alloc : map_) {
      sum += alloc.second;
    }
    return sum;
  }
};

class CustomMemoryPool::CustomMemoryPoolImpl {
 public:
  explicit CustomMemoryPoolImpl(MemoryPool* pool) : pool_(pool) {
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    std::cout << "ACTIVATE_RUNWAY_ALLOCATOR = ON" << std::endl;
#else
    std::cout << "ACTIVATE_RUNWAY_ALLOCATOR = OFF" << std::endl;
#endif
  }

  Status Allocate(int64_t size, uint8_t** out) {
    if (size > 0) {
      std::cout << "Allocate," << size << "," << size << "," << 0 << std::endl;
    }
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    if (size >= HUGE_ALLOC_THRESHOLD_BYTES) {
      return custom_allocate(size, out);
    }
#endif
    return pool_->Allocate(size, out);
  }

  Status Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    if (old_size >= SMALL_ALLOC_THRESHOLD_BYTES ||
        new_size >= HUGE_ALLOC_THRESHOLD_BYTES) {
      // this seems to be meat for the custom allocator
      auto result = custom_reallocate(old_size, new_size, ptr);
      RETURN_NOT_OK(result.status());
      if (result.ValueOrDie()) {
        return Status::OK();
      }
    }
#endif
    uint8_t* previous_ptr = *ptr;
    RETURN_NOT_OK(pool_->Reallocate(old_size, new_size, ptr));
    print_realloc(old_size, new_size, previous_ptr != *ptr);
    return Status::OK();
  }

  void Free(uint8_t* buffer, int64_t size) {
    if (size > 0) {
      std::cout << "Free," << -size << "," << 0 << "," << size << std::endl;
    }
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    if (size >= SMALL_ALLOC_THRESHOLD_BYTES) {
      custom_free(buffer, size);
      return;
    }
#endif
    pool_->Free(buffer, size);
  }

  int64_t bytes_allocated() const {
    return pool_->bytes_allocated() + large_allocs_.sum();
  }

  int64_t max_memory() const { throw "Not implemented yet"; }

  std::string backend_name() const { return pool_->backend_name() + "_custom"; }

  int64_t copied_bytes_;

 private:
  MemoryPool* pool_;
  guarded_map large_allocs_;

  Status custom_allocate(int64_t size, uint8_t** out) {
    void* raw_ptr = mmap(nullptr,  // attribute address automatically
                         HUGE_ALLOC_RUNWAY_SIZE_BYTES, PROT_READ | PROT_WRITE,
                         MAP_ANON | MAP_PRIVATE, 0, 0);
    *out = reinterpret_cast<uint8_t*>(raw_ptr);
    large_allocs_.set(*out, size);
    return Status::OK();
  }

  Result<bool> custom_reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
    uint8_t* previous_ptr = *ptr;
    bool allocation_done = false;
    if (large_allocs_.contains(*ptr)) {
      // already managed by the custom allocator
      if (new_size < SMALL_ALLOC_THRESHOLD_BYTES) {
        // got so small than we return it to inner allocator
        print_realloc(old_size, new_size, true);
        pool_->Allocate(new_size, ptr);
        memcpy(*ptr, previous_ptr, new_size);
        custom_free(previous_ptr, old_size);
      } else {
        print_realloc(old_size, new_size, false);
        large_allocs_.set(*ptr, new_size);
        if (new_size < old_size) {
          // size shrinked so give unused pages back to the OS
          auto new_wps = whole_page_size(new_size);
          // std::cout << "new_wps:" << new_wps << std::endl;
          if (old_size - new_wps > 0) {
            int madv_result = madvise(*ptr + new_wps, old_size - new_wps, MADV_DONTNEED);
            if (madv_result != 0) {
              return Status::ExecutionError("MADV_DONTNEED failed");
            }
          }
        }
      }
      allocation_done = true;
    } else if (new_size >= HUGE_ALLOC_THRESHOLD_BYTES) {
      // from now on manage with custom allocator
      print_realloc(old_size, new_size, true);
      RETURN_NOT_OK(custom_allocate(new_size, ptr));
      memcpy(*ptr, previous_ptr, old_size);
      pool_->Free(previous_ptr, old_size);

      allocation_done = true;
    }
    return allocation_done;
  }

  void custom_free(uint8_t* buffer, int64_t size) {
    auto is_erased = large_allocs_.erase(buffer);
    if (is_erased) {
      munmap(buffer, HUGE_ALLOC_RUNWAY_SIZE_BYTES);
    }
  }

  void print_realloc(int64_t old_size, int64_t new_size, bool is_copy) {
    if (is_copy) {
      copied_bytes_ += old_size;
      std::cout << "ReallocateCopy,";
    } else {
      std::cout << "Reallocate,";
    }
    std::cout << new_size - old_size << "," << new_size << "," << old_size << std::endl;
  }
};

CustomMemoryPool::CustomMemoryPool(MemoryPool* pool) {
  impl_.reset(new CustomMemoryPoolImpl(pool));
}

CustomMemoryPool::~CustomMemoryPool() {}

Status CustomMemoryPool::Allocate(int64_t size, uint8_t** out) {
  return impl_->Allocate(size, out);
}

Status CustomMemoryPool::Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
  return impl_->Reallocate(old_size, new_size, ptr);
}

void CustomMemoryPool::Free(uint8_t* buffer, int64_t size) {
  return impl_->Free(buffer, size);
}

int64_t CustomMemoryPool::bytes_allocated() const { return impl_->bytes_allocated(); }

int64_t CustomMemoryPool::max_memory() const { return impl_->max_memory(); }

int64_t CustomMemoryPool::copied_bytes() const { return impl_->copied_bytes_; }

std::string CustomMemoryPool::backend_name() const { return impl_->backend_name(); }

}  // namespace arrow
