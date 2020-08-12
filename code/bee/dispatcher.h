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

#include "downloader.h"
#include "logger.h"
#include "parquet-helpers.h"
#include "partial-file.h"
#include "preproc-cache.h"
#include "processor.h"

namespace Buzz {

class Dispatcher {
 public:
  Dispatcher(arrow::MemoryPool* mem_pool, const SdkOptions& options,
             int max_concurrent_dl)
      : processor_(mem_pool) {
    synchronizer_ = std::make_shared<Synchronizer>();
    metrics_manager_ = std::make_shared<MetricsManager>();

    downloader_ = std::make_shared<Downloader>(synchronizer_, max_concurrent_dl,
                                               metrics_manager_, options);
  }

  Status execute(std::vector<S3Path> files, const Query& query) {
    for (auto& file : files) {
      // TODO max number of simulteanous footer dl ?
      DownloadFooter(downloader_, file);
    }
    // Process results
    int col_chuncks_to_dl = 0;
    int col_chuncks_downloaded = 0;
    int footers_to_dl = files.size();
    int footers_downloaded = 0;
    PreprocCache preproc_cache;
    metrics_manager_->NewEvent("start_scheduler");
    while (col_chuncks_downloaded < col_chuncks_to_dl ||
           footers_downloaded < footers_to_dl) {
      metrics_manager_->EnterPhase("wait_dl");
      synchronizer_->wait();
      metrics_manager_->ExitPhase("wait_dl");

      auto chunck_files = GetChunckFiles(downloader_);
      for (auto& chunck_file : chunck_files) {
        if (chunck_file.row_group == -1) {
          //// PROCESS FOOTER DOWNLOAD ////
          footers_downloaded++;
          auto file_metadata = processor_.ReadMetadata(chunck_file.file);

          auto col_phys_plans = ColumnPhysicalPlans::Make(query, file_metadata);

          preproc_cache.AddMetadata(chunck_file.path.ToString(), file_metadata,
                                    col_phys_plans);

          // Filter row groups with metadata here (ParquetFileFragment::FilterRowGroups())
          // For strings only min/max can be used (dict not available in footer)
          for (int i = 0; i < file_metadata->num_row_groups(); i++) {
            for (auto col_plan : col_phys_plans) {
              auto metadata_preproc =
                  processor_.PreprocessColumnMetadata(col_plan, file_metadata, i);
              if (metadata_preproc.has_value()) {
                preproc_cache.AddColumn(chunck_file.path.ToString(), i, col_plan->col_id,
                                        metadata_preproc.value());
                // TODO process RowGroup if complete
              }
            }
          }

          // If plan cannot be solved from metadata, download column chuncks
          // TODO a more progressive scheduling of new connections
          for (int i = 0; i < file_metadata->num_row_groups(); i++) {
            for (auto col_plan : col_phys_plans) {
              // TODO make sure the column is in the cache for the same query
              if (!preproc_cache
                       .GetColumn(chunck_file.path.ToString(), i, col_plan->col_id)
                       .has_value()) {
                DownloadColumnChunck(downloader_, file_metadata, chunck_file.path, i,
                                     col_plan->col_id);
                col_chuncks_to_dl++;
              }
            }
          }
        } else {
          //// PROCESS COL CHUNCK DOWNLOAD ////
          col_chuncks_downloaded++;
          auto [file_metadata, col_phys_plans] =
              preproc_cache.GetMetadata(chunck_file.path.ToString()).ValueOrDie();

          // pre-process columns
          metrics_manager_->EnterPhase("col_proc");
          metrics_manager_->NewEvent("starting_col_proc");
          ARROW_ASSIGN_OR_RAISE(auto col_proc_result, processor_.PreprocessColumnFile(
                                                          file_metadata, chunck_file));
          metrics_manager_->ExitPhase("col_proc");

          // cache pre-process results
          auto cols_for_rg =
              preproc_cache.AddColumn(chunck_file.path.ToString(), chunck_file.row_group,
                                      chunck_file.column, col_proc_result);

          // process row group once complete
          if (col_phys_plans.check_complete(cols_for_rg)) {
            metrics_manager_->EnterPhase("rg_proc");
            metrics_manager_->NewEvent("starting_rg_proc");
            ARROW_ASSIGN_OR_RAISE(auto preproc_cols,
                                  preproc_cache.GetRowGroup(chunck_file.path.ToString(),
                                                            chunck_file.row_group));
            RETURN_NOT_OK(
                processor_.ProcessRowGroup(file_metadata->RowGroup(chunck_file.row_group),
                                           col_phys_plans, query, preproc_cols));
            metrics_manager_->ExitPhase("rg_proc");
          }
        }
      }
    }
    metrics_manager_->NewEvent("processings_finished");

    std::cout << "downloaded_chuncks:" << col_chuncks_downloaded << std::endl;
    // metrics_manager_->Print();
    return Status::OK();
  }

 private:
  Processor processor_;
  std::shared_ptr<Synchronizer> synchronizer_;
  std::shared_ptr<MetricsManager> metrics_manager_;
  std::shared_ptr<Downloader> downloader_;
};

}  // namespace Buzz
