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

#include <aws/lambda-runtime/runtime.h>

#include <iostream>

#include "bootstrap.h"
#include "cust_memory_pool.h"
#include "dispatcher.h"
#include "downloader.h"
#include "logger.h"
#include "parquet_helper.h"
#include "partial_file.h"
#include "sdk-init.h"
#include "toolbox.h"

using namespace Buzz;

static const int MAX_CONCURRENT_DL = util::getenv_int("MAX_CONCURRENT_DL", 8);
// static const int NB_CONN_INIT = util::getenv_int("NB_CONN_INIT", 1);
static const auto mem_pool = new CustomMemoryPool(arrow::default_memory_pool());
static const bool IS_LOCAL = util::getenv_bool("IS_LOCAL", false);

// January 1, 2020 0:00:00
// static const int64_t START_TS = util::getenv_int("START_TS", 1577836800000);

// flextime
static const int64_t START_TS = util::getenv_int("START_TS", 1596240000000);

// August 1, 2020 0:00:00
static const int64_t END_TS = util::getenv_int("END_TS", 1596240000000);

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req, const SdkOptions& options) {
  Dispatcher dispatcher{mem_pool, options, MAX_CONCURRENT_DL};
  std::cout << "dispatcher.execute" << std::endl;
  Query query{};
  query.compute_count = true;
  query.metrics = {MetricAggregation{AggType::SUM, "cpm"},
                   MetricAggregation{AggType::SUM, "cpmUplift"}};
  query.time_filter = {START_TS, END_TS, "ingestionTime"};
  query.tag_filters = {TagFilter{{"mobile"}, false, "device"}};

  auto result =
      dispatcher.execute({S3Path{"bb-test-data-dev", "bid-large.parquet"}}, query);
  if (!result.ok()) {
    std::cout << "query exec error: " << result.message() << std::endl;
  }
  return aws::lambda_runtime::invocation_response::success("Done", "text/plain");
}

/** LAMBDA MAIN **/
int main() {
  InitializeAwsSdk(AwsSdkLogLevel::Off);
  // init s3 client
  SdkOptions options;
  options.region = "eu-west-1";
  if (IS_LOCAL) {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  bootstrap([&options](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(req, options);
  });
  // this is mainly usefull to avoid Valgrind errors as Lambda do not guaranty the
  // execution of this code before killing the container
  FinalizeAwsSdk();
}
