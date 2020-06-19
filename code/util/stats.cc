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

#include "stats.h"

#include <iostream>

namespace util {
using namespace arrow;

template <typename T>
CountStat<T>::CountStat() : counts_(1000) {}

template <typename T>
void CountStat<T>::Print() const {
  std::cout << "CountStat.size()=" << counts_.size() << std::endl;
  std::cout << "CountStat.bucket_count()=" << counts_.bucket_count() << std::endl;
}

template <typename T>
Status CountStat<T>::Add(T* items, int64_t len) {
  auto end = items + len;
  for (T* item = items; item != end; ++item) {
    counts_[*item]++;
  }
  return Status::OK();
}

template class CountStat<parquet::ByteArray>;
// template class CountStat<parquet::FLBA>;
// template class CountStat<parquet::Int96>;
template class CountStat<float>;
// ...

}  // namespace util