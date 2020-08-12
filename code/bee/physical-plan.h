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

#include <parquet/metadata.h>
#include <query.h>

#include <vector>

namespace Buzz {

/// Caracterize the pre-processing steps
struct ColumnPhysicalPlan {
  std::string col_name;
  int col_id;
  bool is_filter;
  bool is_group_by;
  std::vector<AggType> aggs;
};

class ColumnPhysicalPlans {
 public:
  using ColumnPhysicalPlanPtr = std::shared_ptr<ColumnPhysicalPlan>;

  Result<ColumnPhysicalPlanPtr> GetById(int col_id) {
    for (auto& plan : plans) {
      if (plan->col_id == col_id) {
        return plan;
      }
    }
    return Status::KeyError("id not found");
  }

  Result<ColumnPhysicalPlanPtr> GetByName(std::string col_name) {
    for (auto& plan : plans) {
      if (plan->col_name == col_name) {
        return plan;
      }
    }
    return Status::KeyError("name not found");
  }

  ColumnPhysicalPlanPtr Insert(std::string col_name,
                               std::shared_ptr<parquet::FileMetaData> file_metadata) {
    for (int i; i < file_metadata->schema()->num_columns(); i++) {
      if (col_name == file_metadata->schema()->Column(i)->name()) {
        plans.emplace_back(
            std::make_shared<ColumnPhysicalPlan>(ColumnPhysicalPlan{col_name, i}));
        return plans.at(plans.size() - 1);
      }
    }
  }

  std::vector<ColumnPhysicalPlanPtr>::iterator begin() { return plans.begin(); }

  std::vector<ColumnPhysicalPlanPtr>::iterator end() { return plans.end(); }

  bool check_complete(std::set<int>& cols_for_rg) const {
    for (auto& col_plan : plans) {
      bool col_found = false;
      for (int col_avail : cols_for_rg) {
        if (col_avail == col_plan->col_id) {
          col_found = true;
          break;
        }
      }
      if (!col_found) {
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<ColumnPhysicalPlanPtr> plans;
};

}  // namespace Buzz
