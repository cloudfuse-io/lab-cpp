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

#include <downloader.h>

#include <optional>
#include <vector>

namespace Buzz {

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

}  // namespace Buzz
