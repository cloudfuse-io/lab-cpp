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

#include <gtest/gtest.h>

#include <cstdint>

#include "arrow/memory_pool.h"
#include "arrow/status.h"
#include "arrow/testing/gtest_util.h"

namespace arrow {

#ifdef ACTIVATE_RUNWAY_ALLOCATOR
TEST(CustomMemoryPool, Custom) {
  auto pool = MemoryPool::CreateDefault();
  CustomMemoryPool pp(pool.get());

  // allocations //

  uint8_t* small_data;
  auto small_alloc_sz = arrow::SMALL_ALLOC_THRESHOLD_BYTES / 2;
  ASSERT_OK(pp.Allocate(small_alloc_sz, &small_data));
  ASSERT_EQ(small_alloc_sz, pp.bytes_allocated());

  uint8_t* large_data;
  auto large_alloc_sz = arrow::HUGE_ALLOC_THRESHOLD_BYTES * 2;
  ASSERT_OK(pp.Allocate(large_alloc_sz, &large_data));
  ASSERT_EQ(small_alloc_sz + large_alloc_sz, pp.bytes_allocated());

  // growing //

  uint8_t* small_data_grown = small_data;
  auto huge_realloc_sz = arrow::HUGE_ALLOC_THRESHOLD_BYTES * 3;
  ASSERT_OK(pp.Reallocate(small_alloc_sz, huge_realloc_sz, &small_data_grown));
  // our alloc is copied to runway
  ASSERT_NE(small_data_grown, small_data);
  ASSERT_EQ(large_alloc_sz + huge_realloc_sz, pp.bytes_allocated());

  uint8_t* large_data_grown = large_data;
  ASSERT_OK(pp.Reallocate(large_alloc_sz, huge_realloc_sz, &large_data_grown));
  // our alloc is not copied, already on runway
  ASSERT_EQ(large_data_grown, large_data);
  ASSERT_EQ(2 * huge_realloc_sz, pp.bytes_allocated());

  // shrinking //

  uint8_t* grown_data_shrinked1 = large_data_grown;
  auto small_realloc_sz = arrow::SMALL_ALLOC_THRESHOLD_BYTES / 4;
  ASSERT_OK(pp.Reallocate(huge_realloc_sz, small_realloc_sz, &grown_data_shrinked1));
  // our alloc is copied back to inner allocator
  ASSERT_NE(grown_data_shrinked1, large_data_grown);
  ASSERT_EQ(small_realloc_sz + huge_realloc_sz, pp.bytes_allocated());

  uint8_t* grown_data_shrinked2 = small_data_grown;
  auto large_realloc_sz = arrow::HUGE_ALLOC_THRESHOLD_BYTES * 2;
  ASSERT_OK(pp.Reallocate(huge_realloc_sz, large_realloc_sz, &grown_data_shrinked2));
  // the alloc stays on the runway but memadvice is called
  ASSERT_EQ(grown_data_shrinked2, small_data_grown);
  ASSERT_EQ(small_realloc_sz + large_realloc_sz, pp.bytes_allocated());

  // deallocation //
  pp.Free(grown_data_shrinked1, small_realloc_sz);
  pp.Free(grown_data_shrinked2, large_realloc_sz);

  ASSERT_EQ(0, pp.bytes_allocated());
}
#endif

}  // namespace arrow
