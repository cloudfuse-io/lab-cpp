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

#include <algorithm>
#include <iostream>
#include <map>

#include "metrics.h"
#include "toolbox.h"

namespace util {
using namespace rapidjson;

Logger::Logger(bool is_local) : is_local_(is_local) {
  if (!is_local) {
    // init s3
  }
}

LogEntry::LogEntry(const char* msg, bool is_local)
    : buffer_(), writer_(buffer_), is_local_(is_local) {
  writer_.StartObject();
  writer_.Key("time");
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now());
  auto datestring = arrow_vendored::date::format("%FT%T%z", now);
  writer_.String(datestring.data());
  writer_.Key("msg");
  writer_.String(msg);
}

void LogEntry::StrField(const char* key, const char* value) {
  writer_.Key(key);
  writer_.String(value);
}

void LogEntry::IntField(const char* key, int64_t value) {
  writer_.Key(key);
  writer_.Int64(value);
}

void LogEntry::FloatField(const char* key, double value) {
  writer_.Key(key);
  writer_.Double(value);
}

void LogEntry::Log() {
  writer_.EndObject();
  if (is_local_) {
    std::cout << buffer_.GetString() << std::endl;
  } else {
    // TODO add to s3 buffer intead of logging
    std::cout << buffer_.GetString() << std::endl;
  }
}

}  // namespace util