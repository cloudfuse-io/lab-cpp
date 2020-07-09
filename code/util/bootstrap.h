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

#pragma once

#include <aws/lambda-runtime/runtime.h>

#include <iostream>

#include "toolbox.h"

template <typename Func>
void bootstrap(Func handler) {
  if (util::getenv_bool("IS_LOCAL", false)) {
    aws::lambda_runtime::invocation_response response =
        handler(aws::lambda_runtime::invocation_request());
    std::cout << "response: " << response.get_payload() << std::endl;
  } else {
    aws::lambda_runtime::run_handler(handler);
  }
}