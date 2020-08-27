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

#include <arrow/array/array_base.h>
#include <arrow/scalar.h>

class ConstantArray {
 public:
  ConstantArray(arrow::Scalar value, int64_t length,
                const std::shared_ptr<arrow::Buffer> null_bitmap = NULLPTR,
                int64_t null_count = arrow::kUnknownNullCount)
      : value_(value),
        null_bitmap_data_(null_bitmap),
        null_count_(null_count),
        length_(length) {}

  /// Size in the number of elements this array contains.
  int64_t length() const { return length_; }

  /// \brief Return true if value at index is null. Does not boundscheck
  bool IsNull(int64_t i) const {
    return null_bitmap_data_ != NULLPTR &&
           !arrow::BitUtil::GetBit(null_bitmap_data_->data(), i);
  }

  /// \brief Return true if value at index is valid (not null). Does not
  /// boundscheck
  bool IsValid(int64_t i) const {
    return null_bitmap_data_ == NULLPTR ||
           arrow::BitUtil::GetBit(null_bitmap_data_->data(), i);
  }

  int64_t null_count() const;

 private:
  arrow::Scalar value_;
  const std::shared_ptr<arrow::Buffer>& null_bitmap_data_;
  int64_t null_count_;
  int64_t length_;
};