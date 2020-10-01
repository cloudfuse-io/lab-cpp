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

#include "dispatcher.h"

// #include <physical_plan.h>
// #include <processed_cache.h>

// namespace Buzz {

// Status Dispatcher::execute(std::vector<S3Path> files, const Query& query) {
//   for (auto& file : files) {
//     // TODO max number of simulteanous footer dl ?
//     parquet_helper_->DownloadFooter(file);
//   }
//   // Process results
//   int col_chuncks_to_dl = 0;
//   int col_chuncks_downloaded = 0;
//   int footers_to_dl = files.size();
//   int footers_downloaded = 0;
//   QueryPhysicalPlans query_phys_plans;
//   QueryRowGroupContainers query_rg_containers;
//   BeeResultBuilder bee_res_builder{mem_pool_};
//   metrics_manager_->NewEvent("start_scheduler");
//   while (col_chuncks_downloaded < col_chuncks_to_dl ||
//          footers_downloaded < footers_to_dl) {
//     metrics_manager_->EnterPhase("wait_dl");
//     synchronizer_->wait();
//     metrics_manager_->ExitPhase("wait_dl");

//     auto chunck_files = parquet_helper_->GetChunckFiles();
//     for (auto& chunck_file : chunck_files) {
//       if (chunck_file.is_footer()) {
//         metrics_manager_->EnterPhase("footer_proc");
//         footers_downloaded++;
//         ARROW_ASSIGN_OR_RAISE(auto file_phys_plan,
//                               query_phys_plans.CreateAndAdd(chunck_file, query));

//         for (int row_group = 0; row_group < file_phys_plan->RowGroupCount();
//              row_group++) {
//           auto rg_container = query_rg_containers.CreateAndAdd(
//               chunck_file.location(), file_phys_plan->RowGroupPlan(), row_group);
//           auto column_plans = file_phys_plan->ColumnPlans();
//           for (auto& col_plan : column_plans) {
//             if (rg_container->is_skipped()) {
//               break;
//             }
//             auto opt_preproc_col = col_plan->PreExecute(row_group);
//             if (!opt_preproc_col.has_value()) {
//               // metadata not enough: start download
//               auto s3_path = std::dynamic_pointer_cast<S3Path>(chunck_file.location());
//               if (!s3_path) {
//                 return Status::UnknownError("Downloader only supports S3 locations");
//               }
//               parquet_helper_->DownloadColumnChunck(file_phys_plan->metadata(),
//               *s3_path,
//                                                     row_group, col_plan->column_id());
//               col_chuncks_to_dl++;
//             } else {
//               // metadata contained all the info
//               auto preproc_col = opt_preproc_col.value();
//               rg_container->Add(preproc_col);
//             }
//           }
//         }
//         metrics_manager_->ExitPhase("footer_proc");
//       } else {  // file chunck is a row group
//         col_chuncks_downloaded++;

//         ARROW_ASSIGN_OR_RAISE(
//             auto rg_container,
//             query_rg_containers.Get(chunck_file.location(), chunck_file.row_group()));

//         if (rg_container->is_skipped()) {
//           continue;
//         }

//         ARROW_ASSIGN_OR_RAISE(
//             auto col_phys_plan,
//             query_phys_plans.ColumnPlan(chunck_file.location(), chunck_file.column()));

//         metrics_manager_->EnterPhase("col_proc");
//         metrics_manager_->NewEvent("starting_col_proc");
//         ARROW_ASSIGN_OR_RAISE(
//             auto preproc_col,
//             col_phys_plan->Execute(chunck_file.file(), chunck_file.row_group()));
//         metrics_manager_->ExitPhase("col_proc");

//         rg_container->Add(preproc_col);

//         if (rg_container->is_skipped()) {
//           continue;
//         } else if (rg_container->Ready()) {
//           metrics_manager_->EnterPhase("rg_proc");
//           metrics_manager_->NewEvent("starting_rg_proc");
//           ARROW_ASSIGN_OR_RAISE(auto rg_result, rg_container->Execute());
//           bee_res_builder.Collect(rg_result);
//           metrics_manager_->ExitPhase("rg_proc");
//         }
//       }
//     }
//     metrics_manager_->ExitPhase("wait_dl");
//   }
//   metrics_manager_->NewEvent("processings_finished");

//   std::cout << "downloaded_chuncks:" << col_chuncks_downloaded << std::endl;
//   // metrics_manager_->Print();
//   return Status::OK();
// }

// }  // namespace Buzz