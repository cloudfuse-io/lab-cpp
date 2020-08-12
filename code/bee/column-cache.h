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

#include <arrow/buffer.h>
#include <partial-file.h>

#include <map>
#include <optional>

namespace Buzz {

struct PreprocResult {
  // TODO better materialize preproc result:
  // - only one value for group by
  // - all true filter
  // - all false filter
  bool skippable = false;
  std::shared_ptr<arrow::ChunkedArray> array;
};

class ColumnCache {
 public:
  using Columns = std::map<int, PreprocResult>;
  using RowGroups = std::map<int, Columns>;

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

  /// returns the cols cached for this RG
  std::set<int> AddColumn(std::string file, int rg, int col, PreprocResult chunck) {
    files[file][rg][col] = chunck;
    std::set<int> cols_cached;
    for (auto& col : files[file][rg]) {
      cols_cached.insert(col.first);
    }
    return cols_cached;
  }

  std::optional<PreprocResult> GetColumn(std::string file, int rg, int col) {
    auto file_found = files.find(file);
    if (file_found != files.end()) {
      auto rg_found = file_found->second.find(rg);
      if (rg_found != file_found->second.end()) {
        auto col_found = rg_found->second.find(col);
        if (col_found != rg_found->second.end()) {
          return col_found->second;
        }
      }
    }
    return std::nullopt;
  }

 private:
  std::map<std::string, RowGroups> files;
};

}  // namespace Buzz
