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
#include "async_queue.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

using namespace testing;

TEST(AsyncQueue, IntMonoThread) {
  auto synchronizer = std::make_shared<Synchronizer>();
  auto queue = AsyncQueue<int>(synchronizer, 1);
  queue.PushRequest([]() -> Result<int> { return 2; });
  synchronizer->wait();
  auto responses = queue.PopResponses();
  ASSERT_EQ(responses.size(), 1);
  ASSERT_EQ(responses[0], Result<int>(2));
}

TEST(AsyncQueue, IntMultiThread) {
  auto synchronizer = std::make_shared<Synchronizer>();
  auto queue = AsyncQueue<int>(synchronizer, 4);
  std::vector<Result<int>> expected_response;
  for (int i = 0; i < 20; i++) {
    queue.PushRequest([i]() { return i; });
    expected_response.push_back(Result<int>(i));
  }
  std::vector<Result<int>> processed;
  while (processed.size() < 20) {
    synchronizer->wait();
    auto responses = queue.PopResponses();
    processed.insert(processed.end(), responses.begin(), responses.end());
  }
  ASSERT_THAT(processed, UnorderedElementsAreArray(expected_response));
}

TEST(AsyncQueue, SharedPointer) {
  auto synchronizer = std::make_shared<Synchronizer>();
  auto queue = AsyncQueue<std::shared_ptr<int>>(synchronizer, 1);
  queue.PushRequest([]() { return std::make_shared<int>(2); });
  synchronizer->wait();
  auto responses = queue.PopResponses();
  ASSERT_EQ(responses.size(), 1);
  ASSERT_EQ(responses[0].status(), Status::OK());
  ASSERT_EQ(*(responses[0].ValueOrDie()), 2);
}