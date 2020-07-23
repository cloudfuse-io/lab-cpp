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

#include "partial-file.h"

#include <arrow/buffer.h>
#include <arrow/testing/gtest_util.h>
#include <gtest/gtest.h>

#include <cstdint>

namespace Buzz {
TEST(PartialFile, SingleChunck) {
  auto bytes = arrow::Buffer::FromString("Hello!");
  FileChunck chunck{100, bytes};
  PartialFile file{{chunck}, 1000};
  ASSERT_EQ(file.GetSize(), 1000);
  ASSERT_TRUE(file.ReadAt(0, 10).status().IsIOError());
  auto read_at_res = file.ReadAt(100, bytes->size());
  ASSERT_EQ(read_at_res.status(), Status::OK());
  ASSERT_EQ(read_at_res.ValueOrDie()->size(), bytes->size());
}

}  // namespace Buzz
