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

#include <arrow/vendored/datetime.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <iostream>

#include "toolbox.h"

namespace util {
using namespace rapidjson;

class LogEntry {
 public:
  void StrField(const char* key, const char* value);
  void IntField(const char* key, int64_t value);
  void FloatField(const char* key, double value);
  void Log();
  friend class Logger;

 private:
  LogEntry(const char* msg, time::time_point timestamp, bool is_local);
  StringBuffer buffer_;
  Writer<StringBuffer> writer_;
  bool is_local_;
};

class Logger {
 public:
  Logger(bool is_local);
  LogEntry NewEntry(const char* msg, time::time_point timestamp = time::now()) const {
    return LogEntry(msg, timestamp, is_local_);
  }

 private:
  bool is_local_;
};

}  // namespace util
