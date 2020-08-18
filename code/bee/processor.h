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
#include <arrow/compute/api.h>
#include <parquet/arrow/reader.h>

#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

#include "downloader.h"
#include "kernels.h"
#include "logger.h"
#include "parquet-helper.h"
#include "partial-file.h"
#include "physical-plan.h"
#include "preproc-cache.h"

namespace Buzz {

class Processor {
 public:
  Processor(arrow::MemoryPool* mem_pool) : mem_pool_(mem_pool) {}

  /// Try to solve the query plan directly from metadata
  std::optional<PreprocCache::Column> PreprocessColumnMetadata(
      Buzz::ColumnPhysicalPlans::ColumnPhysicalPlanPtr col_plan,
      std::shared_ptr<parquet::FileMetaData> file_metadata, int row_group) {
    // TODO analyse metadata
    // - only one value for group by
    // - all true filter
    // - all false filter
    return std::nullopt;
  }

  /// Solve the query plan from the actual column
  Result<PreprocCache::Column> PreprocessColumnFile(
      Buzz::ColumnPhysicalPlans::ColumnPhysicalPlanPtr col_plan,
      std::shared_ptr<parquet::FileMetaData> file_metadata,
      ParquetHelper::ChunckFile& chunck_file) {
    ARROW_ASSIGN_OR_RAISE(
        auto raw_arrow,
        read_column_chunck(chunck_file.file, file_metadata, col_plan->read_dict,
                           chunck_file.row_group, chunck_file.column));
    auto result_arrow = raw_arrow;
    Buzz::IndexSets index_sets;
    if (col_plan->create_filter_index) {
      if (col_plan->filter_expression->ToString() == "StringFilter") {
        auto expression =
            std::dynamic_pointer_cast<StringExpression>(col_plan->filter_expression);
        if (!expression) {
          return Status::TypeError("Invalid cast");
        }

        ARROW_ASSIGN_OR_RAISE(index_sets,
                              compute::index_of(raw_arrow, expression->candidates));

        std::cout << "Nb index set chuncks: " << index_sets.sets_for_chuncks.size()
                  << std::endl;
      }
      // WIP
    }
    if (col_plan->create_bitset) {
      if (col_plan->filter_expression->ToString() == "TimeFilter") {
        if (raw_arrow->type()->name() != "timestamp") {
          return Status::TypeError("Timestamp column read as arrow " +
                                   raw_arrow->type()->name());
        }

        auto expression =
            std::dynamic_pointer_cast<TimeExpression>(col_plan->filter_expression);
        auto timestamp_type =
            std::dynamic_pointer_cast<arrow::TimestampType>(raw_arrow->type());
        if (!expression || !timestamp_type) {
          return Status::TypeError("Invalid cast");
        }

        // create lower bound bitset
        ARROW_ASSIGN_OR_RAISE(auto start_scalar,
                              TimePointToScalar(expression->lower, timestamp_type));
        ARROW_ASSIGN_OR_RAISE(
            auto start_result,
            arrow::compute::Compare(raw_arrow, arrow::Datum(start_scalar),
                                    arrow::compute::CompareOptions{
                                        arrow::compute::CompareOperator::GREATER_EQUAL}));

        // create upper bound bitset
        ARROW_ASSIGN_OR_RAISE(auto end_scalar,
                              TimePointToScalar(expression->upper, timestamp_type));
        ARROW_ASSIGN_OR_RAISE(
            auto end_result,
            arrow::compute::Compare(
                raw_arrow, arrow::Datum(end_scalar),
                arrow::compute::CompareOptions{arrow::compute::CompareOperator::LESS}));

        ARROW_ASSIGN_OR_RAISE(auto start_and_end_result,
                              arrow::compute::And(start_result, end_result));

        result_arrow = start_and_end_result.chunked_array();
      }
      // TODO shortcut when all false
    } else if (col_plan->filter_expression->ToString() == "StringFilter") {
      // TODO genereate bitset
    }

    return PreprocCache::Column(result_arrow);
  }

  /// Put preprocessed columns for a rowgroup together and finish query plan
  Status ProcessRowGroup(std::unique_ptr<parquet::RowGroupMetaData> rg_metadata,
                         ColumnPhysicalPlans& col_phys_plans, Query query,
                         PreprocCache::RowGroup preproc_cols) {
    bool is_full_col = true;
    bool skip_rg = false;
    bool has_group_by = false;
    auto filter = PreprocCache::Column{std::make_shared<arrow::BooleanScalar>(true)};
    for (auto& col_plan : col_phys_plans) {
      if (col_plan->filter_expression) {
        is_full_col = false;
        auto col = preproc_cols[col_plan->col_id];
        if (col.are_all_equal() && col.are_all_true()) {
          // this is a noop filter
        } else if (col.are_all_equal()) {
          // this row group can be skipped
          skip_rg = true;
          break;
        } else if (col_plan->create_bitset) {
          // this should be merged to existing filter
          if (filter.are_all_equal() && filter.are_all_true()) {
            filter = PreprocCache::Column(col.get_datum().chunked_array());
          } else {
            ARROW_ASSIGN_OR_RAISE(
                auto merged_datum,
                arrow::compute::And(filter.get_datum(), col.get_datum()));
            filter = PreprocCache::Column(merged_datum.chunked_array());
          }
        }
      }
      if (col_plan->group_by_expression) {
        is_full_col = false;
        has_group_by = true;
      }
    }
    if (skip_rg) {
      if (query.compute_count) {
        std::cout << "COUNT=0" << std::endl;
      }
      for (auto& col_plan : col_phys_plans) {
        for (auto& agg : col_plan->aggs) {
          if (agg == AggType::SUM) {
            std::cout << "SUM(" << col_plan->col_name << ")=0" << std::endl;
          } else {
            std::cout << "UNKOWN_AGG_TYPE" << std::endl;
          }
        }
      }
      return Status::OK();
    } else if (is_full_col) {
      if (query.compute_count) {
        std::cout << "COUNT=" << rg_metadata->num_rows() << std::endl;
      }
      for (auto& col_plan : col_phys_plans) {
        for (auto& agg : col_plan->aggs) {
          auto preproc_res = preproc_cols[col_plan->col_id];
          if (agg == AggType::SUM) {
            ARROW_ASSIGN_OR_RAISE(auto result_datum,
                                  arrow::compute::Sum(preproc_res.get_datum()));
            std::cout << "SUM(" << col_plan->col_name
                      << ")=" << result_datum.scalar()->ToString() << std::endl;
          } else {
            std::cout << "UNKOWN_AGG_TYPE" << std::endl;
          }
        }
      }
    } else {
      // if multiple bitset filters, "and" them
      if (has_group_by) {
        return Status::NotImplemented("Group by not implemented");
      } else {
        std::string count = "";
        for (auto& col_plan : col_phys_plans) {
          for (auto& agg : col_plan->aggs) {
            auto preproc_res = preproc_cols[col_plan->col_id];
            ARROW_ASSIGN_OR_RAISE(
                auto filtered_datum,
                arrow::compute::Filter(preproc_res.get_datum(), filter.get_datum()));
            count = std::to_string(filtered_datum.chunked_array()->length());
            if (agg == AggType::SUM) {
              ARROW_ASSIGN_OR_RAISE(auto result_datum,
                                    arrow::compute::Sum(filtered_datum));
              std::cout << "SUM(" << col_plan->col_name
                        << ")=" << result_datum.scalar()->ToString() << std::endl;
            } else {
              std::cout << "UNKOWN_AGG_TYPE" << std::endl;
            }
          }
        }
        if (query.compute_count) {
          if (count == "") {
            ARROW_ASSIGN_OR_RAISE(auto count_datum,
                                  arrow::compute::Sum(filter.get_datum()));
            count = count_datum.scalar()->ToString();
          }
          std::cout << "COUNT=" << count << std::endl;
        }
      }
    }
    return Status::OK();
  }

  std::shared_ptr<parquet::FileMetaData> ReadMetadata(std::shared_ptr<PartialFile> file) {
    parquet::ReaderProperties props(mem_pool_);
    std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
        parquet::ParquetFileReader::Open(file, props, nullptr);
    return parquet_reader->metadata();
  }

 private:
  arrow::MemoryPool* mem_pool_;

  Result<std::shared_ptr<arrow::ChunkedArray>> read_column_chunck(
      std::shared_ptr<arrow::io::RandomAccessFile> rg_file,
      std::shared_ptr<parquet::FileMetaData> file_metadata, bool is_dict, int rg,
      int col_id) {
    std::unique_ptr<parquet::arrow::FileReader> reader;
    parquet::arrow::FileReaderBuilder builder;
    parquet::ReaderProperties parquet_props(mem_pool_);
    PARQUET_THROW_NOT_OK(builder.Open(rg_file, parquet_props, file_metadata));
    builder.memory_pool(mem_pool_);
    // all strings are viewed as dicts
    auto arrow_props = parquet::ArrowReaderProperties();
    arrow_props.set_read_dictionary(col_id, is_dict);
    builder.properties(arrow_props);
    PARQUET_THROW_NOT_OK(builder.Build(&reader));

    std::shared_ptr<arrow::ChunkedArray> array;
    PARQUET_THROW_NOT_OK(reader->RowGroup(rg)->Column(col_id)->Read(&array));
    return array;
  }

  /// source time in ms since epoch
  Result<std::shared_ptr<arrow::TimestampScalar>> TimePointToScalar(
      int64_t source, std::shared_ptr<arrow::TimestampType> timestamp_type) {
    int64_t target_int;
    if (timestamp_type->unit() == arrow::TimeUnit::SECOND) {
      target_int = source / 1000;
    } else if (timestamp_type->unit() == arrow::TimeUnit::MILLI) {
      target_int = source;
    } else if (timestamp_type->unit() == arrow::TimeUnit::MICRO) {
      target_int = source * 1000;
    } else if (timestamp_type->unit() == arrow::TimeUnit::NANO) {
      target_int = source * 1000000;
    } else {
      return Status::TypeError("Unexpected timestamp unit in arrow array, got " +
                               timestamp_type->unit());
    }
    return std::make_shared<arrow::TimestampScalar>(target_int, timestamp_type);
  }
};

}  // namespace Buzz