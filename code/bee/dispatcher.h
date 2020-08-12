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
#include <vector>

#include "column-cache.h"
#include "downloader.h"
#include "logger.h"
#include "parquet-helpers.h"
#include "partial-file.h"

enum class AggType { SUM };

enum class TimeBucket { HOUR };

struct TimeGrouping {
  TimeBucket bucket;
  std::string col_name;
};

struct MetricAggretation {
  AggType agg_type;
  std::string col_name;
};

struct TagFilter {
  std::vector<std::string> values;
  bool exclude;
  std::string col_name;
};

struct TimeFilter {
  // TODO
};

struct Query {
  S3Path file;
  // aggregs
  bool compute_count;
  std::vector<MetricAggretation> metrics;
  // groupings
  std::vector<std::string> tag_groupings;
  std::optional<TimeGrouping> time_grouping;
  // filters
  std::vector<TagFilter> tag_filters;
  std::optional<TimeFilter> time_filter;
  // limit
  int64_t limit;
};

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

class Dispatcher {
 public:
  Dispatcher(arrow::MemoryPool* mem_pool, const SdkOptions& options,
             int max_concurrent_dl, int nb_con_init) {
    synchronizer_ = std::make_shared<Synchronizer>();
    metrics_manager_ = std::make_shared<util::MetricsManager>();

    downloader_ = std::make_shared<Downloader>(synchronizer_, max_concurrent_dl,
                                               metrics_manager_, options);
    mem_pool_ = mem_pool;
    nb_con_init_ = nb_con_init;
  }

  Status execute(const Query& query) {
    // download the footer of the file queried
    metrics_manager_->EnterPhase("wait_foot");
    auto file_metadata = Buzz::GetMetadata(downloader_, synchronizer_, mem_pool_,
                                           query.file, nb_con_init_);
    metrics_manager_->ExitPhase("wait_foot");

    // TODO plans for filters and group_bys
    ColumnPhysicalPlans col_phys_plans;
    for (auto& metric : query.metrics) {
      auto plan = col_phys_plans.GetByName(metric.col_name);
      if (!plan.ok()) {
        plan = col_phys_plans.Insert(metric.col_name, file_metadata);
      }
      plan.ValueOrDie()->aggs.push_back(metric.agg_type);
    }

    // Filter row groups with metadata here (ParquetFileFragment::FilterRowGroups())
    // For strings only min/max can be used (dict not available in footer)
    Buzz::ColumnCache column_cache;
    for (int i = 0; i < file_metadata->num_row_groups(); i++) {
      for (auto col_plan : col_phys_plans) {
        // TODO analyse metadata
        // - only one value for group by
        // - all true filter
        // - all false filter
        bool skipable = false;
        if (skipable) {
          column_cache.AddColumn(query.file.ToString(), i, col_plan->col_id,
                                 {true, nullptr});
        }
      }
    }

    // If plan cannot be solved from metadata, download column chuncks
    // TODO a more progressive scheduling of new connections
    int chuncks_to_dl = 0;
    for (int i = 0; i < file_metadata->num_row_groups(); i++) {
      for (auto col_plan : col_phys_plans) {
        // TODO make sure the column is in the cache for the same query
        if (!column_cache.GetColumn(query.file.ToString(), i, col_plan->col_id)
                 .has_value()) {
          Buzz::DownloadColumnChunck(downloader_, file_metadata, query.file, i,
                                     col_plan->col_id);
          chuncks_to_dl++;
        }
      }
    }

    int downloaded_chuncks = 0;
    metrics_manager_->NewEvent("start_scheduler");
    while (downloaded_chuncks < chuncks_to_dl) {
      metrics_manager_->EnterPhase("wait_dl");
      synchronizer_->wait();
      metrics_manager_->ExitPhase("wait_dl");

      auto col_chunck_files = Buzz::GetColumnChunckFiles(downloader_);
      downloaded_chuncks += col_chunck_files.size();
      for (auto& col_chunck_file : col_chunck_files) {
        metrics_manager_->EnterPhase("col_proc");
        metrics_manager_->NewEvent("starting_col_proc");
        ARROW_ASSIGN_OR_RAISE(
            auto raw_arrow,
            read_column_chunck(col_chunck_file.file, file_metadata,
                               col_chunck_file.row_group, col_chunck_file.column));
        // TODO complete other steps of the physical plan
        metrics_manager_->ExitPhase("col_proc");

        auto cols_for_rg =
            column_cache.AddColumn(query.file.ToString(), col_chunck_file.row_group,
                                   col_chunck_file.column, {false, raw_arrow});
        if (col_phys_plans.check_complete(cols_for_rg)) {
          metrics_manager_->EnterPhase("rg_proc");
          metrics_manager_->NewEvent("starting_rg_proc");
          bool is_full_col = true;
          bool skip_rg = false;
          for (auto col_id : cols_for_rg) {
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
              std::cout << "COUNT="
                        << file_metadata->RowGroup(col_chunck_file.row_group)->num_rows()
                        << std::endl;
            }
            for (auto& col_plan : col_phys_plans) {
              for (auto& agg : col_plan->aggs) {
                auto preproc_res =
                    column_cache
                        .GetColumn(query.file.ToString(), col_chunck_file.row_group,
                                   col_plan->col_id)
                        .value();
                if (agg == AggType::SUM) {
                  auto column_datum = arrow::Datum(preproc_res.array);
                  ARROW_ASSIGN_OR_RAISE(auto result_datum,
                                        arrow::compute::Sum(column_datum));
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
          metrics_manager_->ExitPhase("rg_proc");
        }
      }
    }
    metrics_manager_->NewEvent("processings_finished");

    std::cout << "downloaded_chuncks:" << downloaded_chuncks << std::endl;
    return Status::OK();
  }

 private:
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

  arrow::MemoryPool* mem_pool_;
  std::shared_ptr<Synchronizer> synchronizer_;
  std::shared_ptr<util::MetricsManager> metrics_manager_;
  std::shared_ptr<Downloader> downloader_;
  int nb_con_init_;
};
