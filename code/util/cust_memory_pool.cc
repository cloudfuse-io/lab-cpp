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
#include <queue>

#include "arrow/memory_pool.h"
#include "arrow/result.h"
#include "arrow/status.h"

static constexpr int64_t PREALLOC_SIZE_BYTES = 1024 * 1024;
static constexpr int64_t PREALLOC_COUNT = 1500;

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

class linked_set {
  using Node = std::pair<uint8_t*, int64_t>;

 private:
  std::vector<std::vector<Node>> vect_vect_;
  mutable std::mutex mutex_;

 public:
  void add(uint8_t* old_key, uint8_t* new_key, int64_t value) {
#ifdef ACTIVATE_ALLOCATION_LINKING
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& node_vect : vect_vect_) {
      if (node_vect[node_vect.size() - 1].first == old_key) {
        node_vect.push_back({new_key, value});
        return;
      }
    }
    std::vector<Node> new_node_vect;
    new_node_vect.push_back({new_key, value});
    vect_vect_.push_back(new_node_vect);
#endif
  }

  void print() const {
#ifdef ACTIVATE_ALLOCATION_LINKING
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto node_vect : vect_vect_) {
      uint8_t* prev_addr = nullptr;
      int64_t prev_size = 0;
      for (auto node : node_vect) {
        std::string sep;
        if (node.second >= prev_size) {
          sep = "+";
        } else {
          sep = "-";
        }
        if (node.first == prev_addr) {
          std::cout << sep;
        } else {
          std::cout << sep << sep;
        }
        std::cout << node.second;
        prev_addr = node.first;
        prev_size = node.second;
      }
      std::cout << std::endl;
    }
#endif
  }
};

class guarded_queue {
 private:
  std::queue<uint8_t*> queue_;
  mutable std::mutex mutex_;

 public:
  uint8_t* pop() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (queue_.size() == 0) {
      return nullptr;
    }
    auto res = queue_.front();
    queue_.pop();
    return res;
  }

  void push(uint8_t* key) {
    std::lock_guard<std::mutex> lk(mutex_);
    queue_.push(key);
  }
};

class CustomMemoryPool::CustomMemoryPoolImpl {
 public:
  explicit CustomMemoryPoolImpl(MemoryPool* pool) : pool_(pool) {
#ifdef ACTIVATE_ALLOCATION_LINKING
    std::cout << "ACTIVATE_ALLOCATION_LINKING = ON" << std::endl;
#else
    std::cout << "ACTIVATE_ALLOCATION_LINKING = OFF" << std::endl;
#endif
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    std::cout << "ACTIVATE_RUNWAY_ALLOCATOR = ON" << std::endl;
#else
    std::cout << "ACTIVATE_RUNWAY_ALLOCATOR = OFF" << std::endl;
#endif
#ifdef ACTIVATE_POOL_ALLOCATOR
    for (int i = 0; i < PREALLOC_COUNT; i++) {
      void* raw_ptr =
          mmap(nullptr,  // attribute address automatically
               PREALLOC_SIZE_BYTES, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
      pool_allocs_.push(reinterpret_cast<uint8_t*>(raw_ptr));
      memset(raw_ptr, 1, static_cast<size_t>(PREALLOC_SIZE_BYTES));
    }
    std::cout << "ACTIVATE_POOL_ALLOCATOR = ON" << std::endl;
#else
    std::cout << "ACTIVATE_POOL_ALLOCATOR = OFF" << std::endl;
#endif
  }

  Status Allocate(int64_t size, uint8_t** out) {
#ifdef ACTIVATE_ALLOCATION_PRINTING
    if (size > 0) {
      std::cout << "Allocate," << size << "," << size << "," << 0 << std::endl;
    }
#endif
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    if (size >= HUGE_ALLOC_THRESHOLD_BYTES) {
      auto status = runway_allocate(size, out);
      linked_allocs_.add(nullptr, *out, size);
      return status;
    }
#endif
#ifdef ACTIVATE_POOL_ALLOCATOR
    if (size > 0) {
      auto status = pool_allocate(size, out);
      linked_allocs_.add(nullptr, *out, size);
      return status;
    }
#endif
    auto status = pool_->Allocate(size, out);
    linked_allocs_.add(nullptr, *out, size);
    return status;
  }

  Status Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
    uint8_t* previous_ptr = *ptr;
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    if (old_size >= SMALL_ALLOC_THRESHOLD_BYTES ||
        new_size >= HUGE_ALLOC_THRESHOLD_BYTES) {
      // this seems to be meat for the custom allocator
      auto result = runway_reallocate(old_size, new_size, ptr);
      RETURN_NOT_OK(result.status());
      if (result.ValueOrDie()) {
        linked_allocs_.add(previous_ptr, *ptr, new_size);
        print_realloc(old_size, new_size, previous_ptr, *ptr);
        return Status::OK();
      }
    }
#endif
#ifdef ACTIVATE_POOL_ALLOCATOR
    if (new_size > 0) {
      // this seems to be meat for the custom allocator
      auto result = pool_reallocate(old_size, new_size, ptr);
      RETURN_NOT_OK(result.status());
      if (result.ValueOrDie()) {
        linked_allocs_.add(previous_ptr, *ptr, new_size);
        print_realloc(old_size, new_size, previous_ptr, *ptr);
        return Status::OK();
      }
    }
#endif
    RETURN_NOT_OK(pool_->Reallocate(old_size, new_size, ptr));
    linked_allocs_.add(previous_ptr, *ptr, new_size);
    print_realloc(old_size, new_size, previous_ptr, *ptr);
    return Status::OK();
  }

  void Free(uint8_t* buffer, int64_t size) {
#ifdef ACTIVATE_ALLOCATION_PRINTING
    if (size > 0) {
      std::cout << "Free," << -size << "," << 0 << "," << size << std::endl;
    }
#endif
#ifdef ACTIVATE_RUNWAY_ALLOCATOR
    if (size >= SMALL_ALLOC_THRESHOLD_BYTES) {
      runway_free(buffer, size);
      return;
    }
#endif
#ifdef ACTIVATE_POOL_ALLOCATOR
    if (size > 0) {
      pool_free(buffer, size);
      return;
    }
#endif
    pool_->Free(buffer, size);
  }

  int64_t bytes_allocated() const {
    linked_allocs_.print();
    return pool_->bytes_allocated() + large_allocs_.sum();
  }

  int64_t max_memory() const { throw "Not implemented yet."; }

  std::string backend_name() const { return pool_->backend_name() + "_custom"; }

  int64_t copied_bytes_;

 private:
  MemoryPool* pool_;
  guarded_map large_allocs_;
  linked_set linked_allocs_;
  guarded_queue pool_allocs_;

  Status runway_allocate(int64_t size, uint8_t** out) {
    void* raw_ptr = mmap(nullptr,  // attribute address automatically
                         HUGE_ALLOC_RUNWAY_SIZE_BYTES, PROT_READ | PROT_WRITE,
                         MAP_ANON | MAP_PRIVATE, 0, 0);
    *out = reinterpret_cast<uint8_t*>(raw_ptr);
    large_allocs_.set(*out, size);
    return Status::OK();
  }

  Status pool_allocate(int64_t size, uint8_t** out) {
    if (size > PREALLOC_SIZE_BYTES) {
      return Status::ExecutionError("Allocation larger than PREALLOC_SIZE_BYTES");
    }
    auto alloc = pool_allocs_.pop();
    if (alloc != nullptr) {
      *out = alloc;
      return Status::OK();
    } else {
      return Status::ExecutionError("no more alloc in pool");
    }
  }

  Result<bool> runway_reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
    uint8_t* previous_ptr = *ptr;
    bool allocation_done = false;
    if (large_allocs_.contains(*ptr)) {
      // already managed by the custom allocator
      if (new_size < SMALL_ALLOC_THRESHOLD_BYTES) {
        // got so small than we return it to inner allocator
        pool_->Allocate(new_size, ptr);
        memcpy(*ptr, previous_ptr, new_size);
        runway_free(previous_ptr, old_size);
      } else {
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
      RETURN_NOT_OK(runway_allocate(new_size, ptr));
      memcpy(*ptr, previous_ptr, old_size);
      pool_->Free(previous_ptr, old_size);

      allocation_done = true;
    }
    return allocation_done;
  }

  Result<bool> pool_reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
    if (old_size == 0) {
      RETURN_NOT_OK(pool_allocate(new_size, ptr));
    } else if (new_size > PREALLOC_SIZE_BYTES) {
      return Status::ExecutionError("Reallocation larger than PREALLOC_SIZE_BYTES");
    }
    return true;
  }

  void runway_free(uint8_t* buffer, int64_t size) {
    auto is_erased = large_allocs_.erase(buffer);
    if (is_erased) {
      munmap(buffer, HUGE_ALLOC_RUNWAY_SIZE_BYTES);
    }
  }

  void pool_free(uint8_t* buffer, int64_t size) { pool_allocs_.push(buffer); }

  void print_realloc(int64_t old_size, int64_t new_size, uint8_t* old_ptr,
                     uint8_t* new_ptr) {
#ifdef ACTIVATE_ALLOCATION_PRINTING
    if (old_ptr != new_ptr) {
      copied_bytes_ += old_size;
      std::cout << "ReallocateCopy,";
    } else {
      std::cout << "Reallocate,";
    }
    std::cout << new_size - old_size << "," << new_size << "," << old_size << std::endl;
#endif
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
