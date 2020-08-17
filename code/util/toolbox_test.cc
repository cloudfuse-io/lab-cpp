// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements. See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership. The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.
#include "toolbox.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

namespace Buzz {

using namespace testing;

TEST(Toolbox, get_duration_ms) {
  auto current_time = time::now();
  ASSERT_EQ(util::get_duration_ms(current_time, current_time + std::chrono::minutes(1)),
            60 * 1000);
}

TEST(Toolbox, get_duration_micro) {
  auto current_time = time::now();
  ASSERT_EQ(
      util::get_duration_micro(current_time, current_time + std::chrono::minutes(1)),
      60 * 1000 * 1000);
}

}  // namespace Buzz
