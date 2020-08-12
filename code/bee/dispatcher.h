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
#include "executor.h"
#include "logger.h"
#include "parquet-helpers.h"
#include "partial-file.h"

namespace Buzz {

class Dispatcher {
 public:
  Dispatcher(arrow::MemoryPool* mem_pool, const SdkOptions& options,
             int max_concurrent_dl, int nb_con_init) {
    synchronizer_ = std::make_shared<Synchronizer>();
    metrics_manager_ = std::make_shared<MetricsManager>();

    downloader_ = std::make_shared<Downloader>(synchronizer_, max_concurrent_dl,
                                               metrics_manager_, options);
    mem_pool_ = mem_pool;
    nb_con_init_ = nb_con_init;
  }

  Status execute(const Query& query) {
    // download the footer of the file queried
    metrics_manager_->EnterPhase("wait_foot");
    auto file_metadata =
        GetMetadata(downloader_, synchronizer_, mem_pool_, query.file, nb_con_init_);
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
    ColumnCache column_cache;
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
          // TODO process RowGroup if complete
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
          DownloadColumnChunck(downloader_, file_metadata, query.file, i,
                               col_plan->col_id);
          chuncks_to_dl++;
        }
      }
    }

    // Process results
    int downloaded_chuncks = 0;
    metrics_manager_->NewEvent("start_scheduler");
    while (downloaded_chuncks < chuncks_to_dl) {
      metrics_manager_->EnterPhase("wait_dl");
      synchronizer_->wait();
      metrics_manager_->ExitPhase("wait_dl");

      auto col_chunck_files = GetColumnChunckFiles(downloader_);
      downloaded_chuncks += col_chunck_files.size();
      for (auto& col_chunck_file : col_chunck_files) {
        // pre-process columns
        metrics_manager_->EnterPhase("col_proc");
        metrics_manager_->NewEvent("starting_col_proc");
        ARROW_ASSIGN_OR_RAISE(
            auto col_proc_result,
            PreprocessColumn(mem_pool_, file_metadata, col_chunck_file));
        metrics_manager_->ExitPhase("col_proc");

        // cache pre-process results
        auto cols_for_rg =
            column_cache.AddColumn(query.file.ToString(), col_chunck_file.row_group,
                                   col_chunck_file.column, col_proc_result);

        // process row group once complete
        if (col_phys_plans.check_complete(cols_for_rg)) {
          metrics_manager_->EnterPhase("rg_proc");
          metrics_manager_->NewEvent("starting_rg_proc");
          ARROW_ASSIGN_OR_RAISE(
              auto preproc_cols,
              column_cache.GetRowGroup(query.file.ToString(), col_chunck_file.row_group));
          RETURN_NOT_OK(ProcessRowGroup(
              mem_pool_, file_metadata->RowGroup(col_chunck_file.row_group),
              col_phys_plans, query, preproc_cols));
          metrics_manager_->ExitPhase("rg_proc");
        }
      }
    }
    metrics_manager_->NewEvent("processings_finished");

    std::cout << "downloaded_chuncks:" << downloaded_chuncks << std::endl;
    metrics_manager_->Print();
    return Status::OK();
  }

 private:
  arrow::MemoryPool* mem_pool_;
  std::shared_ptr<Synchronizer> synchronizer_;
  std::shared_ptr<MetricsManager> metrics_manager_;
  std::shared_ptr<Downloader> downloader_;
  int nb_con_init_;
};

}  // namespace Buzz
