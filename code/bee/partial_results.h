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

#include <arrow/util/hashing.h>
#include <processed_column.h>

#include "arrow/array/array_binary.h"

// QueryResult
// - TableBuilder (metric_defs[])
// - map<std::string, set<int>> row_groups_collected

// QueryResult.Collect(ProcessedRowGroup)
//            .Finalize() -> arrow::Table (schema + ChunckedArray[])

// TableBuilder
// - ArrayBuilder[] group_bys
// - ArrayBuilder[] metrics
// - MetricDefs[] (col_id,AggName)
// - Memoization

namespace Buzz {

class RowGroupResult {
 public:
  RowGroupResult(arrow::MemoryPool* mem_pool) : group_by_key{mem_pool, 0} {}

 private:
  std::shared_ptr<FileLocation> file_location;
  int row_group_id;

  std::vector<arrow::Scalar> total_aggs;  // NumericScalars
  arrow::internal::BinaryMemoTable<arrow::BinaryBuilder> group_by_key;
  std::vector<arrow::ArrayBuilder> metric_data;  // NumericBuilders
};

class BeeResultBuilder {
 public:
  BeeResultBuilder(arrow::MemoryPool* mem_pool) : group_by_key{mem_pool, 0} {}
  void Collect(RowGroupResult processed_row_group);
  BeeResult Finalize();

 private:
  std::vector<arrow::Scalar> total_aggs;  // NumericScalars
  arrow::internal::BinaryMemoTable<arrow::BinaryBuilder> group_by_key;
  std::vector<arrow::ArrayBuilder> metric_data;  // NumericBuilders
};

class BeeResult {
 private:
  arrow::BinaryArray group_by_keys;
  std::vector<arrow::Array> metrics;
};

}  // namespace Buzz
