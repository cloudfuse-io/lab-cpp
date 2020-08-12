// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <future>
#include <iostream>

#include "bootstrap.h"
#include "cust_memory_pool.h"
#include "downloader.h"
#include "logger.h"
#include "partial-file.h"
#include "sdk-init.h"
#include "toolbox.h"

namespace Buzz {

namespace {

struct RequestWeakHash {
  std::size_t operator()(const DownloadRequest& a) const {
    return std::hash<int>()(a.range_end);
  }
};

struct RequestEqual {
  bool operator()(const DownloadRequest& a, const DownloadRequest& b) const {
    return a.path.bucket == b.path.bucket && a.path.key == b.path.key &&
           a.range_start == b.range_start && a.range_end == b.range_end;
  }
};

struct ParquetColumnChunckIds {
  int row_group;
  int column;
};

}  // namespace

std::unordered_map<DownloadRequest, ParquetColumnChunckIds, RequestWeakHash, RequestEqual>
    rg_start_map;

std::shared_ptr<parquet::FileMetaData> GetMetadata(
    std::shared_ptr<Downloader> downloader, std::shared_ptr<Synchronizer> synchronizer,
    arrow::MemoryPool* mem_pool, S3Path path, int nb_init) {
  downloader->ScheduleDownload({std::nullopt, 64 * 1024, path});

  downloader->InitConnections(path.bucket, nb_init);

  DownloadResponse footer_response{{}, nullptr, 0};
  while (footer_response.file_size == 0) {
    synchronizer->wait();
    auto results = downloader->ProcessResponses();
    for (auto& result : results) {
      auto response = result.ValueOrDie();
      if (response.request.range_start.value_or(0) != 0 ||
          response.request.range_end != 0) {
        footer_response = response;
      }
    }
  }
  auto footer_start_pos = footer_response.file_size - footer_response.request.range_end;
  std::vector<Buzz::FileChunck> footer_chuncks{
      {footer_start_pos, footer_response.raw_data}};
  auto footer_file =
      std::make_shared<Buzz::PartialFile>(footer_chuncks, footer_response.file_size);

  // setup raw reader for footers
  parquet::ReaderProperties props(mem_pool);
  std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
      parquet::ParquetFileReader::Open(footer_file, props, nullptr);

  // Get the File MetaData
  std::shared_ptr<parquet::FileMetaData> file_metadata = parquet_reader->metadata();
  std::cout << "file_metadata->num_rows:" << file_metadata->num_rows() << std::endl;

  return file_metadata;
}

void DownloadColumnChunck(std::shared_ptr<Downloader> downloader,
                          std::shared_ptr<parquet::FileMetaData> file_metadata,
                          S3Path path, int row_group, int column) {
  auto col_chunck_meta = file_metadata->RowGroup(row_group)->ColumnChunk(column);
  auto col_chunck_start = col_chunck_meta->file_offset();
  auto col_chunck_end = col_chunck_start + col_chunck_meta->total_compressed_size();
  downloader->ScheduleDownload({col_chunck_start, col_chunck_end, path});
  rg_start_map.emplace(DownloadRequest{col_chunck_start, col_chunck_end, path},
                       ParquetColumnChunckIds{row_group, column});
}

struct ColChunckFile {
  int row_group;
  int column;
  std::shared_ptr<Buzz::PartialFile> file;
};

std::vector<ColChunckFile> GetColumnChunckFiles(std::shared_ptr<Downloader> downloader) {
  auto results = downloader->ProcessResponses();
  std::vector<ColChunckFile> rg_files;
  for (auto& result : results) {
    if (result.status().message() == STATUS_ABORTED.message()) {
      continue;
    }
    auto response = result.ValueOrDie();
    if (response.request.range_start.value_or(0) == 0 &&
        response.request.range_end == 0) {
      continue;
    }
    std::vector<Buzz::FileChunck> rg_chuncks{
        {response.request.range_start.value(), response.raw_data}};
    auto chunck_ids = rg_start_map[response.request];
    rg_files.push_back(
        {chunck_ids.row_group, chunck_ids.column,
         std::make_shared<Buzz::PartialFile>(rg_chuncks, response.file_size)});
  }
  return rg_files;
}

}  // namespace Buzz
