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

void Synchronizer::notify() {
  {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->work_++;
  }
  this->cv_.notify_one();
}

void Synchronizer::wait() {
  std::unique_lock<std::mutex> lock(this->mutex_);
  this->cv_.wait(lock, [this]() { return this->work_ > 0; });
}

void Synchronizer::consume(int work_units) {
  std::unique_lock<std::mutex> lock(this->mutex_);
  this->work_ -= work_units;
}