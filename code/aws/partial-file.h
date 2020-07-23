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

#include <arrow/io/interfaces.h>
#include <result.h>

namespace Buzz {

struct FileChunck {
  int64_t start_position;
  std::shared_ptr<arrow::Buffer> data;
};

/// An in memory file from already loaded memory chuncks
/// Fail if reading a range that was not loaded
class PartialFile : public arrow::io::RandomAccessFile {
 public:
  PartialFile(std::vector<FileChunck> chuncks, int64_t size);

  Result<int64_t> GetSize() override;
  Status Close() override;
  bool closed() const override;
  Result<std::shared_ptr<arrow::Buffer>> ReadAt(int64_t position,
                                                int64_t nbytes) override;

  //// V not used by parquet? only to comply with poor class design? V ////

  Result<int64_t> ReadAt(int64_t position, int64_t nbytes, void* out) override;
  Result<int64_t> Tell() const override;
  Result<int64_t> Read(int64_t nbytes, void* out) override;
  Result<std::shared_ptr<arrow::Buffer>> Read(int64_t nbytes) override;
  Status Seek(int64_t position) override;
  arrow::Future<std::shared_ptr<arrow::Buffer>> ReadAsync(int64_t position,
                                                          int64_t nbytes) override;

 private:
  std::vector<FileChunck> chuncks_;
  int64_t size_;
  int64_t position_;
};

}  // namespace Buzz