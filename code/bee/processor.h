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

  Result<PreprocCache::Column> PreprocessColumnFile(
      std::shared_ptr<parquet::FileMetaData> file_metadata,
      ParquetHelper::ChunckFile& chunck_file) {
    ARROW_ASSIGN_OR_RAISE(auto raw_arrow,
                          read_column_chunck(chunck_file.file, file_metadata,
                                             chunck_file.row_group, chunck_file.column));
    // TODO complete other steps of the physical plan
    return PreprocCache::Column{false, raw_arrow};
  }

  Status ProcessRowGroup(std::unique_ptr<parquet::RowGroupMetaData> rg_metadata,
                         ColumnPhysicalPlans& col_phys_plans, Query query,
                         PreprocCache::RowGroup preproc_cols) {
    bool is_full_col = true;
    bool skip_rg = false;
    for (auto& col_plan : col_phys_plans) {
      // if is filter and all false  => skip_col=true + break
      // if is filter and not all true => is_full_col=false
      // if is group_by and nb_values > 1 => is_full_col=false
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
            auto column_datum = arrow::Datum(preproc_res.array);
            ARROW_ASSIGN_OR_RAISE(auto result_datum, arrow::compute::Sum(column_datum));
            std::cout << "SUM(" << col_plan->col_name
                      << ")=" << result_datum.scalar()->ToString() << std::endl;
          } else {
            std::cout << "UNKOWN_AGG_TYPE" << std::endl;
          }
        }
      }
    } else {
      // if multiple bitset filters, "and" them
      // if no group by create a mock one
      // run hash aggreg (merging with bitset filter)
      return Status::NotImplemented("Filters and GroupBy not implem");
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
      std::shared_ptr<parquet::FileMetaData> file_metadata, int rg, int col_id) {
    std::unique_ptr<parquet::arrow::FileReader> reader;
    parquet::arrow::FileReaderBuilder builder;
    parquet::ReaderProperties parquet_props(mem_pool_);
    PARQUET_THROW_NOT_OK(builder.Open(rg_file, parquet_props, file_metadata));
    builder.memory_pool(mem_pool_);
    // all strings are viewed as dicts
    auto arrow_props = parquet::ArrowReaderProperties();
    bool is_dict = file_metadata->schema()->Column(col_id)->logical_type()->is_string();
    arrow_props.set_read_dictionary(col_id, is_dict);
    builder.properties(arrow_props);
    PARQUET_THROW_NOT_OK(builder.Build(&reader));

    std::shared_ptr<arrow::ChunkedArray> array;
    PARQUET_THROW_NOT_OK(reader->RowGroup(rg)->Column(col_id)->Read(&array));
    return array;
  }
};

}  // namespace Buzz