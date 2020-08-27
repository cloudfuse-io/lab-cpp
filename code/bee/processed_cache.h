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

#include <partial_results.h>
#include <physical_plan.h>
#include <processed_column.h>

namespace Buzz {

class RowGroupContainer {
 public:
  RowGroupContainer(std::shared_ptr<FileLocation> file_location,
                    std::shared_ptr<RowGroupPhysicalPlan> row_group_plan,
                    int row_group_id)
      : file_location_(std::move(file_location)),
        row_group_plan_(std::move(row_group_plan)),
        row_group_id_(row_group_id) {}

 public:
  void Add(ProcessedColumn processed_col);
  bool Ready();
  bool is_skipped() { return skip; }
  void set_skipped() { skip = true; }
  RowGroupResult Execute();

 private:
  std::shared_ptr<RowGroupPhysicalPlan> row_group_plan_;
  int row_group_id_;
  std::shared_ptr<FileLocation> file_location_;
  std::unordered_set<int> columns_;
  bool skip = false;

  std::shared_ptr<FilterColumn> filter_column;
  std::vector<GroupByColumn> group_by_columns;
  std::vector<MetricColumn> metric_columns;
};

}  // namespace Buzz

// #include <arrow/api.h>
// #include <physical-plan.h>
// #include <errors.h>

// #include <map>
// #include <set>

// namespace Buzz {

// class RowGroupResultSet {
//  public:
//   /// check proproced type and send store appropriatly
//   Status AddPreprocedCol(PreprocedCol col);

//   /// Call if this rg can be completely discarded
//   void MarkSkipped() { is_skipped = true; }

//   /// Check if this rg needs to be considered
//   bool IsSkipped() { return is_skipped; }

//  private:
//   void AddBitset(int col_id, std::shared_ptr<arrow::ChunkedArray> array);
//   void AddDictionaryArray(int col_id, std::shared_ptr<arrow::ChunkedArray> array,
//                           IndexSets index_sets);
//   void AddBasicArray(int col_id, std::shared_ptr<arrow::ChunkedArray> array);
//   bool is_skipped = false;
// };

// class PreprocCache {
//  public:
//   struct File {
//     ColumnPhysicalPlans col_phys_plans;
//     std::shared_ptr<parquet::FileMetaData> metadata;
//     std::map<int, std::shared_ptr<RowGroupResultSet>> row_groups;
//   };

//   // int64_t GetSize() {
//   //   int64_t size = 0;
//   //   for (auto& file : files) {
//   //     for (auto& rg : file.second) {
//   //       for (auto& col : rg.second) {
//   //         size += col.second->chunk(0)->->size();
//   //       }
//   //     }
//   //   }
//   // }

//   void AddMetadata(std::string file, std::shared_ptr<parquet::FileMetaData> metadata,
//                    ColumnPhysicalPlans col_phys_plans) {
//     files[file] = {col_phys_plans, metadata, {}};
//   }

//   Result<std::shared_ptr<RowGroupResultSet>> GetRowGroup(std::string file, int rg) {
//     auto file_found = files.find(file);
//     if (file_found != files.end()) {
//       auto rg_found = file_found->second.row_groups.find(rg);
//       if (rg_found != file_found->second.row_groups.end()) {
//         return rg_found->second;
//       }
//     }
//     return Status::KeyError("RowGroup not found in cache");
//   }

//   Result<std::tuple<std::shared_ptr<parquet::FileMetaData>, ColumnPhysicalPlans>>
//   GetMetadata(std::string file) {
//     auto file_found = files.find(file);
//     if (file_found != files.end()) {
//       return std::make_tuple(file_found->second.metadata,
//                              file_found->second.col_phys_plans);
//     }
//     return Status::KeyError("File not found in cache");
//   }

//  private:
//   std::map<std::string, File> files;
// };

// }  // namespace Buzz
