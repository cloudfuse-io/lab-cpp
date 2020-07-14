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

#include <iostream>

enum class AwsSdkLogLevel : int8_t { Off, Fatal, Error, Warn, Info, Debug, Trace };

void InitializeAwsSdk(const AwsSdkLogLevel& options);

void FinalizeAwsSdk();

/// Options for sdk clients.
struct SdkOptions {
  /// AWS region to connect to (default "us-east-1")
  std::string region = "us-east-1";

  /// If non-empty, override region with a connect string such as "localhost:9000"
  // XXX perhaps instead take a URL like "http://localhost:9000"?
  std::string endpoint_override;
  /// S3 connection transport, default "https"
  std::string scheme = "https";
};
