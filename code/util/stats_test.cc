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

#include "stats.h"

#include <arrow/status.h>
#include <arrow/testing/gtest_util.h>
#include <gtest/gtest.h>

#include <cstdint>

namespace util {

TEST(HashingTraits, Compare) {
  HashingTraits<parquet::ByteArray>::Equal byte_array_comparator;
  uint8_t lhs_array[3] = {1, 2, 3};
  auto lhs_ba = parquet::ByteArray{3, lhs_array};

  uint8_t rhs_array_equal[3] = {1, 2, 3};
  auto rhs_ba_equal = parquet::ByteArray{3, rhs_array_equal};
  ASSERT_TRUE(byte_array_comparator(lhs_ba, rhs_ba_equal));

  uint8_t rhs_array_ne[3] = {3, 2, 3};
  auto rhs_ba_ne = parquet::ByteArray{3, rhs_array_ne};
  ASSERT_FALSE(byte_array_comparator(lhs_ba, rhs_ba_ne));
}

}  // namespace util
