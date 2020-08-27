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

#include <array_constant.h>
#include <file_location.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Buzz {

class ProcessedColumn {
  int column_id() { return column_id_; }

 protected:
  int column_id_;
};

class BooleanColumn : public ProcessedColumn {
  // variant<ConstantArray/ChunckedArray>
};

class TimestampColumn : public ProcessedColumn {
  // filter
  // bucket
  // variant<ConstantArray/ChunckedArray>
};

class StringDictColumn : public ProcessedColumn {
  // filter
  // variant<ConstantArray/ChunckedArray>
};

class Int64Column : public ProcessedColumn {
  // variant<ConstantArray/ChunckedArray>
};

// TODO float, int32...

class FilterColumn {
 public:
  FilterColumn(BooleanColumn col);

  bool Get(int64_t index);
  FilterColumn merge(FilterColumn other);

 private:
  enum class Type { Boolean };
  BooleanColumn boolean_;
};

class GroupByColumn {
 public:
  GroupByColumn(TimestampColumn col);
  GroupByColumn(StringDictColumn col);

  /// copy the data at the given index to the buffer with the given offset
  /// returns the number of bytes copied, or 0 if null
  int32_t IndexToBuffer(int32_t index, std::shared_ptr<arrow::Buffer> buffer,
                        int32_t offset);

 private:
  enum class Type { Timestamp, StringDict };
  TimestampColumn timestamp_;
  StringDictColumn string_;
};

class MetricColumn {
 public:
  MetricColumn(Int64Column col);

  // increment for add aggregator
  void Add(arrow::Scalar targetScalar, int64_t sourceIndex);
  void Add(arrow::Array targetArray, int64_t targetIndex, int64_t sourceIndex);

 private:
  enum class Type { Int64 };
  Int64Column int64_;
};

}  // namespace Buzz
