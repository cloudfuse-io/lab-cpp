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
#include <errors.h>

#include <iostream>
#include <unordered_set>
#include <vector>

namespace Buzz {

struct IndexSets {
  std::vector<std::unordered_set<int64_t>> sets_for_chuncks;
};

namespace compute {

Result<IndexSets> index_of(arrow::Datum dict_datum, std::vector<std::string> candidates) {
  if (!dict_datum.is_arraylike()) {
    return Status::TypeError("String column not a chuncked column");
  }
  auto dict_chuncked_array = dict_datum.chunked_array();

  if (dict_chuncked_array->type()->name() != "dictionary") {
    return Status::TypeError("String column read as arrow " +
                             dict_chuncked_array->type()->name());
  }

  IndexSets index_sets;
  for (auto chunck : dict_chuncked_array->chunks()) {
    auto dict_array = std::dynamic_pointer_cast<arrow::DictionaryArray>(chunck);
    if (!dict_array) {
      return Status::TypeError("Invalid cast");
    }

    auto dict_array_dict =
        std::dynamic_pointer_cast<arrow::StringArray>(dict_array->dictionary());
    if (!dict_array_dict) {
      return Status::TypeError("Invalid cast");
    }

    std::unordered_set<int64_t> index_set;
    index_set.reserve(candidates.size());
    for (auto& candidate : candidates) {
      // Note: dict can contain the string more than once
      for (int64_t index = 0; index < dict_array_dict->length(); index++) {
        if (candidate == dict_array_dict->GetView(index)) {
          index_set.insert(index);
        }
      }
    }
    index_sets.sets_for_chuncks.push_back(std::move(index_set));
  }
  return index_sets;
}

}  // namespace compute

}  // namespace Buzz
