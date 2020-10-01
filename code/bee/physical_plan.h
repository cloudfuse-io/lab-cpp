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

#include <errors.h>
#include <file_location.h>
#include <parquet/metadata.h>
#include <partial_file.h>
#include <partial_results.h>
#include <processed_column.h>
#include <query.h>

#include <unordered_map>
#include <vector>

namespace Buzz {

// TODO destructors to avoid leak because of circular refs

class RowGroupPhysicalPlan;
class FilePhysicalPlan;
class QueryPhysicalPlans;

class FilterPlan {
 public:
  virtual Result<arrow::Datum> execute(arrow::Datum) const = 0;
};

class StringFilterPlan : public FilterPlan {
 public:
  StringFilterPlan(std::vector<std::string> values, bool exclude)
      : values_(std::move(values)), exclude_(exclude) {}

  Result<arrow::Datum> execute(arrow::Datum) const override;

 private:
  std::vector<std::string> values_;
  bool exclude_;
};

class TimestampFilterPlan : public FilterPlan {
 public:
  TimestampFilterPlan(int64_t lower_limit_ms, int64_t upper_limit_ms)
      : lower_limit_ms_(lower_limit_ms), upper_limit_ms_(upper_limit_ms) {}
  Result<arrow::Datum> execute(arrow::Datum) const override;

 private:
  int64_t lower_limit_ms_;
  int64_t upper_limit_ms_;
};

class ColumnPhysicalPlan {
 public:
  static Result<std::shared_ptr<ColumnPhysicalPlan>> Make(
      std::shared_ptr<RowGroupPhysicalPlan>, int col_id);

  /// Try to get the ProcessedColumn from metadata only
  /// For strings only min/max can be used (dict not available in footer)
  /// Can be done on creation
  // PreExecute();

  Status Execute(std::shared_ptr<PartialFile> file);

  int column_id() { return column_id_; }

 private:
  int column_id_;
  std::string column_name_;
  bool read_dict_;
  bool create_bitset_;
  std::shared_ptr<FilterPlan> filter_plan_;
  // TODO bucket_plan_ ?
  std::shared_ptr<RowGroupPhysicalPlan> rg_phys_plan_;
  // - expected_type (arrow)
};

///////////////////////////

class RowGroupPhysicalPlan {
 public:
  static Result<std::shared_ptr<RowGroupPhysicalPlan>> Make(
      std::shared_ptr<FilePhysicalPlan> file_phys_plan, int rg_id);

  /// All the column for the row group plan were added
  bool Ready();

  bool Skipped();

 private:
  std::shared_ptr<FilePhysicalPlan> file_phys_plan__;

  std::vector<std::shared_ptr<ColumnPhysicalPlan>> col_phys_plans_;

  int row_group_id_;

  // std::unordered_set<int> columns_;
  // std::vector<int> group_bys_;
  // std::vector<int> metrics_;
  // std::unordered_set<int> filters_;
};

///////////////////////////

class FilePhysicalPlan {
 public:
  static Result<std::shared_ptr<FilePhysicalPlan>> Make(
      std::shared_ptr<QueryPhysicalPlans> query_phys_plan,
      ParquetPartialFileWrap footer_file);

  // std::vector<std::shared_ptr<ColumnPhysicalPlan>> ColumnPlans(int rg_id);
  Result<std::shared_ptr<ColumnPhysicalPlan>> GetColumnPlan(int rg_id, int col_id);
  // std::shared_ptr<RowGroupPhysicalPlan> RowGroupPlan(int rg_id);
  std::shared_ptr<parquet::FileMetaData> metadata() { return metadata_; }
  std::shared_ptr<FileLocation> location() { return file_location_; }

 private:
  std::shared_ptr<parquet::FileMetaData> metadata_;
  std::shared_ptr<FileLocation> file_location_;
  std::vector<std::shared_ptr<RowGroupPhysicalPlan>> row_group_plans_;
};

/////////////////////////

struct ChunckToDownload {
  std::shared_ptr<FileLocation> file_location;
  int64_t chunck_start;
  int64_t chunck_end;
  int row_group_id;
  int column_id;
};

class QueryPhysicalPlans {
 public:
  QueryPhysicalPlans(std::shared_ptr<Query> query) : query_(query) {}

  /// Create a plan for the file from the footer passed as param
  Result<std::vector<ChunckToDownload>> NewFilePlan(ParquetPartialFileWrap footer_file);

  // Result<std::shared_ptr<ColumnPhysicalPlan>> GetColumnPlan(
  //     std::shared_ptr<FileLocation> location, int rg_id, int col_id);

  /// Find the right plan for the file col chunck and execute it
  Status ProcessColumnChunck(ParquetPartialFileWrap col_chunck_file);

  /// Process the next row group that is ready
  /// Return null if no row group is complete and ready to process
  Result<std::shared_ptr<RowGroupResult>> ProcessNextRowGroup();

 private:
  std::vector<std::shared_ptr<FilePhysicalPlan>> file_plans_;
  std::shared_ptr<Query> query_;
};

}  // namespace Buzz

// namespace Buzz {

// class PreprocedCol {
//   // void AddBitset(int col_id, std::shared_ptr<arrow::ChunkedArray> array);
//   // void AddDictionaryArray(int col_id, std::shared_ptr<arrow::ChunkedArray> array,
//   //                         IndexSets index_sets);
//   // void AddBasicArray(int col_id, std::shared_ptr<arrow::ChunkedArray> array);
// }

// struct FilterExpression {
//   virtual std::string ToString() const = 0;
// };

// /// lower and upper are ms since epoch
// struct TimeExpression : public FilterExpression {
//   int64_t lower;
//   int64_t upper;

//   TimeExpression(int64_t start, int64_t end) : lower(start), upper(end) {}

//   std::string ToString() const override { return "TimeFilter"; }
// };

// /// lower and upper are ms since epoch
// struct StringExpression : public FilterExpression {
//   std::vector<std::string> candidates;
//   bool is_exclude;

//   StringExpression(std::vector<std::string> values, bool exclude)
//       : candidates(std::move(values)), is_exclude(exclude) {}

//   std::string ToString() const override { return "StringFilter"; }
// };

// struct GroupByExpression {};

// /// Caracterize the pre-processing steps
// struct ColumnPhysicalPlan {
//   std::string col_name;
//   int col_id;
//   bool read_dict;
//   bool create_filter_index;
//   bool create_bitset;

//   // the selection step of the plan
//   std::shared_ptr<FilterExpression> filter_expression;

//   // the group by step of the plan
//   std::shared_ptr<GroupByExpression> group_by_expression;

//   // bool is_filter;
//   // bool is_group_by;
//   std::vector<AggType> aggs;

//   PreprocedCol execute(std::shared_ptr<arrow::ChunkedArray> arrow_col);
// };

// /// A column by column set of physical plans
// class ColumnPhysicalPlans {
//  public:
//   using ColumnPhysicalPlanPtr = std::shared_ptr<ColumnPhysicalPlan>;

//   static Result<ColumnPhysicalPlans> Make(
//       Query query, std::shared_ptr<parquet::FileMetaData> file_metadata) {
//     ColumnPhysicalPlans col_phys_plans;
//     // TODO plans for filters and group_bys
//     for (auto& metric : query.metrics) {
//       auto plan_res = col_phys_plans.GetByName(metric.col_name);
//       if (!plan_res.ok()) {
//         plan_res = col_phys_plans.Insert(metric.col_name, file_metadata);
//       }
//       auto plan_ptr = plan_res.ValueOrDie();
//       auto parquet_log_type =
//           file_metadata->schema()->Column(plan_ptr->col_id)->logical_type();
//       // logical type often left "none" when physical type is clear primitive
//       // TODO check the exact rule for this
//       if (!parquet_log_type->is_int() && !parquet_log_type->is_decimal() &&
//           !parquet_log_type->is_none()) {
//         return Status::ExpressionValidationError(
//             "Metric should by a Parquet numeric column, got ",
//             parquet_log_type->ToString());
//       }
//       plan_ptr->aggs.push_back(metric.agg_type);
//       plan_ptr->read_dict = false;
//       plan_ptr->create_filter_index = false;
//       plan_ptr->create_bitset = false;
//     }
//     if (query.time_filter.has_value()) {
//       auto filter = query.time_filter.value();
//       auto plan_res = col_phys_plans.GetByName(filter.col_name);
//       if (!plan_res.ok()) {
//         plan_res = col_phys_plans.Insert(filter.col_name, file_metadata);
//       }
//       auto plan_ptr = plan_res.ValueOrDie();
//       auto parquet_log_type =
//           file_metadata->schema()->Column(plan_ptr->col_id)->logical_type();
//       if (!parquet_log_type->is_timestamp()) {
//         return Status::ExpressionValidationError(
//             "Time filter should be a Parquet timestamp column, got ",
//             parquet_log_type->ToString());
//       }
//       plan_ptr->read_dict = false;
//       plan_ptr->create_filter_index = false;
//       plan_ptr->create_bitset = true;
//       plan_ptr->filter_expression =
//           std::make_shared<TimeExpression>(filter.start, filter.end);
//     }
//     for (auto& filter : query.tag_filters) {
//       auto plan_res = col_phys_plans.GetByName(filter.col_name);
//       if (!plan_res.ok()) {
//         plan_res = col_phys_plans.Insert(filter.col_name, file_metadata);
//       }
//       auto plan_ptr = plan_res.ValueOrDie();
//       auto parquet_log_type =
//           file_metadata->schema()->Column(plan_ptr->col_id)->logical_type();
//       if (!parquet_log_type->is_string()) {
//         return Status::ExpressionValidationError(
//             "Filter should be a Parquet string column, got ",
//             parquet_log_type->ToString());
//       }
//       plan_ptr->read_dict = true;
//       plan_ptr->create_filter_index = true;
//       plan_ptr->create_bitset = true;  // TODO false if group by
//       plan_ptr->filter_expression =
//           std::make_shared<StringExpression>(filter.values, filter.exclude);
//     }
//     return col_phys_plans;
//   }

//   Result<ColumnPhysicalPlanPtr> GetById(int col_id) {
//     for (auto& plan : plans) {
//       if (plan->col_id == col_id) {
//         return plan;
//       }
//     }
//     return Status::KeyError("id not found");
//   }

//   Result<ColumnPhysicalPlanPtr> GetByName(std::string col_name) {
//     for (auto& plan : plans) {
//       if (plan->col_name == col_name) {
//         return plan;
//       }
//     }
//     return Status::KeyError("name not found");
//   }

//   std::vector<ColumnPhysicalPlanPtr>::iterator begin() { return plans.begin(); }

//   std::vector<ColumnPhysicalPlanPtr>::iterator end() { return plans.end(); }

//   /// check that the set of column ids contain all the necessary columns for this plan
//   set bool check_complete(std::set<int>& cols_for_rg) const {
//     for (auto& col_plan : plans) {
//       bool col_found = false;
//       for (int col_avail : cols_for_rg) {
//         if (col_avail == col_plan->col_id) {
//           col_found = true;
//           break;
//         }
//       }
//       if (!col_found) {
//         return false;
//       }
//     }
//     return true;
//   }

//  private:
//   std::vector<ColumnPhysicalPlanPtr> plans;

//   ColumnPhysicalPlanPtr Insert(std::string col_name,
//                                std::shared_ptr<parquet::FileMetaData> file_metadata) {
//     for (int i; i < file_metadata->schema()->num_columns(); i++) {
//       if (col_name == file_metadata->schema()->Column(i)->name()) {
//         plans.emplace_back(
//             std::make_shared<ColumnPhysicalPlan>(ColumnPhysicalPlan{col_name, i}));
//         return plans.at(plans.size() - 1);
//       }
//     }
//   }
// };

// }  // namespace Buzz
