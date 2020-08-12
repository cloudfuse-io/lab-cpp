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
#include <physical-plan.h>
#include <result.h>

#include <map>
#include <set>

namespace Buzz {

class PreprocCache {
 public:
  struct Column {
    // TODO better materialize preproc result:
    // - only one value for group by
    // - all true filter
    // - all false filter
    bool skippable = false;
    std::shared_ptr<arrow::ChunkedArray> array;
  };
  using RowGroup = std::map<int, Column>;
  struct File {
    ColumnPhysicalPlans col_phys_plans;
    std::shared_ptr<parquet::FileMetaData> metadata;
    std::map<int, RowGroup> row_groups;
  };

  // int64_t GetSize() {
  //   int64_t size = 0;
  //   for (auto& file : files) {
  //     for (auto& rg : file.second) {
  //       for (auto& col : rg.second) {
  //         size += col.second->chunk(0)->->size();
  //       }
  //     }
  //   }
  // }

  void AddMetadata(std::string file, std::shared_ptr<parquet::FileMetaData> metadata,
                   ColumnPhysicalPlans col_phys_plans) {
    files[file] = {col_phys_plans, metadata, {}};
  }

  /// returns the cols cached for this RG
  std::set<int> AddColumn(std::string file, int rg, int col, Column col_preproc) {
    files[file].row_groups[rg][col] = col_preproc;
    std::set<int> cols_cached;
    for (auto& col : files[file].row_groups[rg]) {
      cols_cached.insert(col.first);
    }
    return cols_cached;
  }

  std::optional<Column> GetColumn(std::string file, int rg, int col) {
    auto file_found = files.find(file);
    if (file_found != files.end()) {
      auto rg_found = file_found->second.row_groups.find(rg);
      if (rg_found != file_found->second.row_groups.end()) {
        auto col_found = rg_found->second.find(col);
        if (col_found != rg_found->second.end()) {
          return col_found->second;
        }
      }
    }
    return std::nullopt;
  }

  Result<RowGroup> GetRowGroup(std::string file, int rg) {
    auto file_found = files.find(file);
    if (file_found != files.end()) {
      auto rg_found = file_found->second.row_groups.find(rg);
      if (rg_found != file_found->second.row_groups.end()) {
        return rg_found->second;
      }
    }
    return Status::KeyError("RowGroup not found in cache");
  }

  Result<std::shared_ptr<parquet::FileMetaData>> GetMetadata(std::string file) {
    auto file_found = files.find(file);
    if (file_found != files.end()) {
      return file_found->second.metadata;
    }
    return Status::KeyError("File not found in cache");
  }

  Result<ColumnPhysicalPlans> GetPhysicalPlans(std::string file) {
    auto file_found = files.find(file);
    if (file_found != files.end()) {
      return file_found->second.col_phys_plans;
    }
    return Status::KeyError("File not found in cache");
  }

 private:
  std::map<std::string, File> files;
};

}  // namespace Buzz
