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

#pragma once

#include <arrow/api.h>
#include <query.h>

#include "parquet_helper.h"

namespace Buzz {

class Dispatcher {
 public:
  Dispatcher(arrow::MemoryPool* mem_pool, const SdkOptions& options,
             int max_concurrent_dl)
      : mem_pool_(mem_pool),
        synchronizer_(std::make_shared<Synchronizer>()),
        metrics_manager_(std::make_shared<MetricsManager>()),
        parquet_helper_(std::make_unique<ParquetHelper>(std::make_shared<Downloader>(
            synchronizer_, max_concurrent_dl, metrics_manager_, options))) {}

  Status execute(std::vector<S3Path> files, const Query& query);

 private:
  arrow::MemoryPool* mem_pool_;
  std::shared_ptr<Synchronizer> synchronizer_;
  std::shared_ptr<MetricsManager> metrics_manager_;
  std::unique_ptr<ParquetHelper> parquet_helper_;
};

}  // namespace Buzz
