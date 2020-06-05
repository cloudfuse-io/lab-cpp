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

#include "arrow/memory_pool.h"
#include "arrow/status.h"

namespace arrow {

constexpr int64_t HUGE_ALLOC_THRESHOLD_BYTES = 1024 * 1024;
constexpr int64_t SMALL_ALLOC_THRESHOLD_BYTES = 256 * 1024;
constexpr int64_t HUGE_ALLOC_RUNWAY_SIZE_BYTES = 1024 * 1024 * 1024;

class CustomMemoryPool::CustomMemoryPoolImpl {
 public:
  explicit CustomMemoryPoolImpl(MemoryPool* pool) : pool_(pool) {}

  // Status Allocate(int64_t size, uint8_t** out) {
  //   if (size > 0) {
  //     std::cout << "Allocate," << size << "," << size << std::endl;
  //   }
  //   if (size > HUGE_ALLOC_THRESHOLD_BYTES) {
  //     void* raw_ptr =
  //         mmap(nullptr,  // position auto attributed
  //              HUGE_ALLOC_RUNWAY_SIZE_BYTES, PROT_READ | PROT_WRITE | PROT_EXEC,
  //              MAP_ANON | MAP_PRIVATE,  // to a private block of hardware memory
  //              0, 0);
  //     *out = reinterpret_cast<uint8_t*>(raw_ptr);
  //     large_allocs[*out] = size;
  //   } else {
  //     RETURN_NOT_OK(pool_->Allocate(size, out));
  //   }

  //   return Status::OK();
  // }

  // Status Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
  //   uint8_t* previous_ptr = *ptr;
  //   if (old_size >= SMALL_ALLOC_THRESHOLD_BYTES ||
  //       new_size >= HUGE_ALLOC_THRESHOLD_BYTES) {
  //     // this seems to be meet for the custom allocator
  //     if (large_allocs.find(*ptr) != large_allocs.end()) {
  //       // already managed by the custom allocator
  //       if (new_size < SMALL_ALLOC_THRESHOLD_BYTES) {
  //         // got so small than we return it to jemalloc
  //         pool_->Allocate(new_size, ptr);
  //         memcpy(*ptr, previous_ptr, new_size);
  //         Free(previous_ptr, old_size);
  //       } else {
  //         large_allocs[*ptr] = new_size;
  //         if (new_size < old_size) {
  //           // size shrinked so give bytes back to the OS
  //           int madv_result =
  //               madvise(*ptr + new_size, old_size - new_size, MADV_DONTNEED);
  //           if (madv_result != 0) {
  //             return Status::ExecutionError("MADV_DONTNEED failed");
  //           }
  //         }
  //       }
  //     } else if (new_size > HUGE_ALLOC_THRESHOLD_BYTES) {
  //       // not yet managed by the custom allocator but should be
  //       RETURN_NOT_OK(Allocate(new_size, ptr));
  //       memcpy(*ptr, previous_ptr, new_size);
  //       pool_->Free(previous_ptr, old_size);
  //     }
  //     return Status::OK();
  //   }

  //   RETURN_NOT_OK(pool_->Reallocate(old_size, new_size, ptr));
  //   if (previous_ptr != *ptr) {
  //     std::cout << "ReallocateCopy,";
  //   } else {
  //     std::cout << "Reallocate,";
  //   }
  //   std::cout << new_size - old_size << "," << new_size << std::endl;
  //   return Status::OK();
  // }

  // void Free(uint8_t* buffer, int64_t size) {
  //   if (size > 0) {
  //     std::cout << "Free," << -size << "," << 0 << std::endl;
  //   }
  //   if (size > SMALL_ALLOC_THRESHOLD_BYTES) {
  //     auto is_erased = large_allocs.erase(buffer);
  //     if (is_erased) {
  //       munmap(buffer, HUGE_ALLOC_RUNWAY_SIZE_BYTES);
  //     }
  //     return;
  //   }
  //   pool_->Free(buffer, size);
  // }

  Status Allocate(int64_t size, uint8_t** out) {
    if (size > 0) {
      std::cout << "Allocate," << size << "," << size << std::endl;
    }
    RETURN_NOT_OK(pool_->Allocate(size, out));

    return Status::OK();
  }

  Status Reallocate(int64_t old_size, int64_t new_size, uint8_t** ptr) {
    uint8_t* previous_ptr = *ptr;
    RETURN_NOT_OK(pool_->Reallocate(old_size, new_size, ptr));
    if (previous_ptr != *ptr) {
      std::cout << "ReallocateCopy,";
    } else {
      std::cout << "Reallocate,";
    }
    std::cout << new_size - old_size << "," << new_size << std::endl;
    return Status::OK();
  }

  void Free(uint8_t* buffer, int64_t size) {
    if (size > 0) {
      std::cout << "Free," << -size << "," << 0 << std::endl;
    }
    pool_->Free(buffer, size);
  }

  int64_t bytes_allocated() const { return 0; }

  int64_t max_memory() const { return 0; }

  std::string backend_name() const { return pool_->backend_name() + "_custom"; }

 private:
  MemoryPool* pool_;
  std::map<uint8_t*, int64_t> large_allocs;
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

std::string CustomMemoryPool::backend_name() const { return impl_->backend_name(); }

}  // namespace arrow
