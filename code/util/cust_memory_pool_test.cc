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

#include <gtest/gtest.h>

#include <cstdint>

#include "arrow/memory_pool.h"
#include "arrow/status.h"
#include "arrow/testing/gtest_util.h"

namespace arrow {

TEST(CustomMemoryPool, Custom) {
  auto pool = MemoryPool::CreateDefault();
  ProxyMemoryPool pp(pool.get());

  uint8_t* data;
  ASSERT_OK(pool->Allocate(100, &data));

  uint8_t* data2;
  ASSERT_OK(pp.Allocate(300, &data2));

  ASSERT_EQ(400, pool->bytes_allocated());
  ASSERT_EQ(300, pp.bytes_allocated());

  pool->Free(data, 100);
  pp.Free(data2, 300);

  ASSERT_EQ(0, pool->bytes_allocated());
  ASSERT_EQ(0, pp.bytes_allocated());
}

}  // namespace arrow
