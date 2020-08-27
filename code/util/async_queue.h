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

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "errors.h"

namespace Buzz {

/// Synchronize the results of multiple AsyncQueues together
class Synchronizer {
 public:
  void notify();
  void wait();
  void consume(int work_units);

 private:
  std::condition_variable cv_;
  std::mutex mutex_;
  // TODO count work by notifiers
  int work_;
};

template <typename ResponseType>
class AsyncQueue {
 public:
  using RequestType = std::function<Result<ResponseType>()>;

  AsyncQueue(std::shared_ptr<Synchronizer> synchronizer, int pool_size);
  ~AsyncQueue();

  void PushRequest(RequestType request);

  std::vector<Result<ResponseType>> PopResponses();

  void PushResponse(Result<ResponseType> response);

 private:
  // input queue
  std::queue<std::function<void()>> request_queue_;
  std::mutex request_queue_mutex_;
  std::condition_variable request_cv_;

  // output queue
  std::queue<Result<ResponseType>> resp_queue_;
  std::mutex resp_queue_mutex_;
  std::shared_ptr<Synchronizer> synchronizer_;

  std::vector<std::thread> workers_;
  bool stop_;
};

//// AsyncQueue HEADER ONLY BECAUSE OF TEMPLATING ////

template <typename ResponseType>
AsyncQueue<ResponseType>::AsyncQueue(std::shared_ptr<Synchronizer> synchronizer,
                                     int pool_size)
    : synchronizer_(synchronizer), stop_(false) {
  for (size_t i = 0; i < pool_size; ++i)
    workers_.emplace_back([this] {
      for (;;) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(this->request_queue_mutex_);
          this->request_cv_.wait(
              lock, [this] { return this->stop_ || !this->request_queue_.empty(); });
          if (this->stop_ && this->request_queue_.empty()) return;
          task = std::move(this->request_queue_.front());
          this->request_queue_.pop();
        }
        task();
      }
    });
}

template <typename ResponseType>
void AsyncQueue<ResponseType>::PushRequest(AsyncQueue::RequestType request_func) {
  {
    std::unique_lock<std::mutex> lock(this->request_queue_mutex_);

    // don't allow enqueueing after stopping the pool
    if (this->stop_) throw std::runtime_error("Queue stopped");

    request_queue_.push([this, request_func]() {
      auto response = request_func();
      {
        std::unique_lock<std::mutex> lock(this->resp_queue_mutex_);
        this->resp_queue_.push(response);
      }
      synchronizer_->notify();
    });
  }
  request_cv_.notify_one();
}

template <typename ResponseType>
std::vector<Result<ResponseType>> AsyncQueue<ResponseType>::PopResponses() {
  std::vector<Result<ResponseType>> result;
  std::unique_lock<std::mutex> lock(this->resp_queue_mutex_);
  result.reserve(this->resp_queue_.size());
  while (!this->resp_queue_.empty()) {
    result.push_back(this->resp_queue_.front());
    this->resp_queue_.pop();
  }
  this->synchronizer_->consume(result.size());
  return result;
}

template <typename ResponseType>
AsyncQueue<ResponseType>::~AsyncQueue() {
  {
    std::unique_lock<std::mutex> lock(request_queue_mutex_);
    this->stop_ = true;
  }
  request_cv_.notify_all();
  for (std::thread& worker : workers_) worker.join();
}

}  // namespace Buzz
