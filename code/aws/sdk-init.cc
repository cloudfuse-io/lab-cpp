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

#include "sdk-init.h"

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

#include <atomic>
#include <mutex>

#include "curl/HttpClientFactory.h"

namespace {

std::mutex aws_init_lock;
Aws::SDKOptions aws_options;
bool aws_initialized = false;

#define LOG_LEVEL_CASE(level_name)                             \
  case AwsSdkLogLevel::level_name:                             \
    aws_log_level = Aws::Utils::Logging::LogLevel::level_name; \
    break;

}  // namespace

void InitializeAwsSdk(const AwsSdkLogLevel& log_level) {
  std::lock_guard<std::mutex> lock(aws_init_lock);
  if (!aws_initialized) {
    Aws::Utils::Logging::LogLevel aws_log_level;
    switch (log_level) {
      LOG_LEVEL_CASE(Fatal)
      LOG_LEVEL_CASE(Error)
      LOG_LEVEL_CASE(Warn)
      LOG_LEVEL_CASE(Info)
      LOG_LEVEL_CASE(Debug)
      LOG_LEVEL_CASE(Trace)
      default:
        aws_log_level = Aws::Utils::Logging::LogLevel::Warn;
    }
    aws_options.loggingOptions.logLevel = aws_log_level;
    // By default the AWS SDK logs to files, log to console instead
    aws_options.loggingOptions.logger_create_fn = [] {
      return std::make_shared<Aws::Utils::Logging::ConsoleLogSystem>(
          aws_options.loggingOptions.logLevel);
    };
    aws_options.httpOptions.httpClientFactory_create_fn = []() {
      return std::make_shared<Buzz::Http::CustomHttpClientFactory>();
    };
    // httpOptions: is setting custom httpClientFactory_create_fn, then initAndCleanupCurl
    // and installSigPipeHandler should by handled by the custom HttpClientFacory
    Aws::InitAPI(aws_options);
    aws_initialized = true;
  }
}

void FinalizeAwsSdk() {
  std::lock_guard<std::mutex> lock(aws_init_lock);
  Aws::ShutdownAPI(aws_options);
  aws_initialized = false;
}
