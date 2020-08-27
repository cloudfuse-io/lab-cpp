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

#include "partial_file.h"

#include <arrow/buffer.h>
#include <arrow/util/future.h>

#include "iostream"

namespace Buzz {

PartialFile::PartialFile(std::vector<FileChunck> chuncks, int64_t size)
    : chuncks_(chuncks), size_(size), position_(0) {
  // TODO use an effecient map to find chuncks ?
}

Result<int64_t> PartialFile::GetSize() { return size_; }

Status PartialFile::Close() { chuncks_.clear(); }

bool PartialFile::closed() const { return chuncks_.size() == 0; };

Result<std::shared_ptr<arrow::Buffer>> PartialFile::ReadAt(int64_t position,
                                                           int64_t nbytes) {
  for (auto& chunck : chuncks_) {
    if (position >= chunck.start_position &&
        position < chunck.start_position + chunck.data->size()) {
      if (position + nbytes > chunck.start_position + chunck.data->size()) {
        // TODO better debug printing
        std::cout << "read " << position << "-" << position + nbytes << std::endl;
        std::cout << "from " << chunck.start_position << "-"
                  << chunck.start_position + chunck.data->size() << std::endl;
        return Status::IOError("chunck too small for read size");
      }
      // return view to chunck
      auto offset = position - chunck.start_position;
      return std::make_shared<arrow::Buffer>(chunck.data, offset, nbytes);
    }
  }
  // TODO better debug printing
  std::cout << "read " << position << "-" << position + nbytes << std::endl;
  std::cout << "from :" << std::endl;
  for (auto& chunck : chuncks_) {
    std::cout << "  " << chunck.start_position << "-"
              << chunck.start_position + chunck.data->size() << std::endl;
  }
  return Status::IOError("chunck not in partial file");
}

//// V not used by parquet? only to comply with poor class design? V ////

Result<int64_t> PartialFile::ReadAt(int64_t position, int64_t nbytes, void* out) {
  return Status::IOError("ReadAt with void* out param not supported");
}

Result<int64_t> PartialFile::Tell() const { return position_; }

Result<int64_t> PartialFile::Read(int64_t nbytes, void* out) {
  return Status::IOError("Read with void* out param not supported");
}

Result<std::shared_ptr<arrow::Buffer>> PartialFile::Read(int64_t nbytes) {
  auto result = ReadAt(position_, nbytes);
  position_ += nbytes;
  return result;
}

Status PartialFile::Seek(int64_t position) {
  position_ = position;
  return Status::OK();
}

}  // namespace Buzz