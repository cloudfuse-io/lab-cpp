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
struct FilterExpression {
  virtual std::string ToString() const = 0;
};

/// lower and upper are ms since epoch
struct TimeExpression : public FilterExpression {
  int64_t lower;
  int64_t upper;

  TimeExpression(int64_t start, int64_t end) : lower(start), upper(end) {}

  std::string ToString() const override { return "TimeFilter"; }
};

/// lower and upper are ms since epoch
struct StringExpression : public FilterExpression {
  std::vector<std::string> candidates;
  bool is_exclude;

  StringExpression(std::vector<std::string> values, bool exclude)
      : candidates(std::move(values)), is_exclude(exclude) {}

  std::string ToString() const override { return "StringFilter"; }
};

struct GroupByExpression {};

/// Caracterize the pre-processing steps
struct ColumnPhysicalPlan {
  std::string col_name;
  int col_id;
  bool read_dict;
  bool create_filter_index;
  bool create_bitset;

  // the selection step of the plan
  std::shared_ptr<FilterExpression> filter_expression;

  // the group by step of the plan
  std::shared_ptr<GroupByExpression> group_by_expression;

  // bool is_filter;
  // bool is_group_by;
  std::vector<AggType> aggs;
};

/// A column by column set of physical plans
class ColumnPhysicalPlans {
 public:
  using ColumnPhysicalPlanPtr = std::shared_ptr<ColumnPhysicalPlan>;

  static Result<ColumnPhysicalPlans> Make(
      Query query, std::shared_ptr<parquet::FileMetaData> file_metadata) {
    ColumnPhysicalPlans col_phys_plans;
    // TODO plans for filters and group_bys
    for (auto& metric : query.metrics) {
      auto plan_res = col_phys_plans.GetByName(metric.col_name);
      if (!plan_res.ok()) {
        plan_res = col_phys_plans.Insert(metric.col_name, file_metadata);
      }
      auto plan_ptr = plan_res.ValueOrDie();
      auto parquet_log_type =
          file_metadata->schema()->Column(plan_ptr->col_id)->logical_type();
      // logical type often left "none" when physical type is clear primitive
      // TODO check the exact rule for this
      if (!parquet_log_type->is_int() && !parquet_log_type->is_decimal() &&
          !parquet_log_type->is_none()) {
        return Status::ExpressionValidationError(
            "Metric should by a Parquet numeric column, got ",
            parquet_log_type->ToString());
      }
      plan_ptr->aggs.push_back(metric.agg_type);
      plan_ptr->read_dict = false;
      plan_ptr->create_filter_index = false;
      plan_ptr->create_bitset = false;
    }
    if (query.time_filter.has_value()) {
      auto filter = query.time_filter.value();
      auto plan_res = col_phys_plans.GetByName(filter.col_name);
      if (!plan_res.ok()) {
        plan_res = col_phys_plans.Insert(filter.col_name, file_metadata);
      }
      auto plan_ptr = plan_res.ValueOrDie();
      auto parquet_log_type =
          file_metadata->schema()->Column(plan_ptr->col_id)->logical_type();
      if (!parquet_log_type->is_timestamp()) {
        return Status::ExpressionValidationError(
            "Time filter should be a Parquet timestamp column, got ",
            parquet_log_type->ToString());
      }
      plan_ptr->read_dict = false;
      plan_ptr->create_filter_index = false;
      plan_ptr->create_bitset = true;
      plan_ptr->filter_expression =
          std::make_shared<TimeExpression>(filter.start, filter.end);
    }
    for (auto& filter : query.tag_filters) {
      auto plan_res = col_phys_plans.GetByName(filter.col_name);
      if (!plan_res.ok()) {
        plan_res = col_phys_plans.Insert(filter.col_name, file_metadata);
      }
      auto plan_ptr = plan_res.ValueOrDie();
      auto parquet_log_type =
          file_metadata->schema()->Column(plan_ptr->col_id)->logical_type();
      if (!parquet_log_type->is_string()) {
        return Status::ExpressionValidationError(
            "Filter should be a Parquet string column, got ",
            parquet_log_type->ToString());
      }
      plan_ptr->read_dict = true;
      plan_ptr->create_filter_index = true;
      plan_ptr->create_bitset = true;  // TODO false if group by
      plan_ptr->filter_expression =
          std::make_shared<StringExpression>(filter.values, filter.exclude);
    }
    return col_phys_plans;
  }

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

  std::vector<ColumnPhysicalPlanPtr>::iterator begin() { return plans.begin(); }

  std::vector<ColumnPhysicalPlanPtr>::iterator end() { return plans.end(); }

  /// check that the set of column ids contain all the necessary columns for this plan set
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
};

}  // namespace Buzz
