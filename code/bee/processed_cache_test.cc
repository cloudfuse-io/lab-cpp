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

#include "processed_cache.h"

#include <arrow/testing/gtest_util.h>
#include <gtest/gtest.h>

#include <cstdint>

namespace Buzz {

TEST(MapFromView, SerializationPlayground) {
  std::shared_ptr<arrow::Buffer> buf = arrow::AllocateBuffer(100).ValueOrDie();
  auto data = buf->mutable_data();
  int64_t timestamp = 191030654613;
  memcpy(data, &timestamp, 8);
  std::string tag = "hello";
  int32_t length = tag.length();
  memcpy(data + 8, &length, 4);
  memcpy(data + 12, tag.data(), length);

  arrow::internal::BinaryMemoTable<arrow::BinaryBuilder> group_by_key{
      arrow::default_memory_pool(), 0};
  int32_t memo_index;
  auto status = group_by_key.GetOrInsert(data, 12 + length, &memo_index);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(memo_index, 0);
}

}  // namespace Buzz
