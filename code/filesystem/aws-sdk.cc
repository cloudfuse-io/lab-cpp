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

#include "aws-sdk.h"

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

#include <atomic>
#include <mutex>

namespace {

std::mutex aws_init_lock;
Aws::SDKOptions aws_options;
std::atomic<bool> aws_initialized(false);

struct AwsSdkGlobalOptions {
  AwsSdkLogLevel log_level;
};

void DoInitialize(const AwsSdkGlobalOptions& options) {
  Aws::Utils::Logging::LogLevel aws_log_level;

#define LOG_LEVEL_CASE(level_name)                             \
  case AwsSdkLogLevel::level_name:                             \
    aws_log_level = Aws::Utils::Logging::LogLevel::level_name; \
    break;

  switch (options.log_level) {
    LOG_LEVEL_CASE(Fatal)
    LOG_LEVEL_CASE(Error)
    LOG_LEVEL_CASE(Warn)
    LOG_LEVEL_CASE(Info)
    LOG_LEVEL_CASE(Debug)
    LOG_LEVEL_CASE(Trace)
    default:
      aws_log_level = Aws::Utils::Logging::LogLevel::Off;
  }

#undef LOG_LEVEL_CASE

  aws_options.loggingOptions.logLevel = aws_log_level;
  // By default the AWS SDK logs to files, log to console instead
  aws_options.loggingOptions.logger_create_fn = [] {
    return std::make_shared<Aws::Utils::Logging::ConsoleLogSystem>(
        aws_options.loggingOptions.logLevel);
  };
  Aws::InitAPI(aws_options);
  aws_initialized.store(true);
}

}  // namespace

void InitializeAwsSdk(const AwsSdkLogLevel& options) {
  std::lock_guard<std::mutex> lock(aws_init_lock);
  if (!aws_initialized.load()) {
    AwsSdkGlobalOptions options{AwsSdkLogLevel::Fatal};
    return DoInitialize(options);
  }
}

void FinalizeAwsSdk() {
  std::lock_guard<std::mutex> lock(aws_init_lock);
  Aws::ShutdownAPI(aws_options);
  aws_initialized.store(false);
}
