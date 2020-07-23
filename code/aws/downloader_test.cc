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

#include "downloader.cc"

#include <arrow/testing/gtest_util.h>
#include <gtest/gtest.h>

#include <cstdint>

TEST(Helpers, FormatRange) {
  ASSERT_EQ(FormatRange(0, 10), "bytes=0-10");
  ASSERT_EQ(FormatRange(std::nullopt, 10), "bytes=-10");
}

TEST(Helpers, ParseRange) {
  ASSERT_TRUE(ParseRange("yolo").status().IsIOError());
  ASSERT_EQ(ParseRange("bytes 500-1233/1234"), Result<int64_t>(1234));
}

TEST(Helpers, CalculateLength) {
  ASSERT_EQ(CalculateLength(0, 10), 11);
  ASSERT_EQ(CalculateLength(std::nullopt, 10), 10);
}
