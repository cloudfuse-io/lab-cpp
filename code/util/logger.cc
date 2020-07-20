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

#include "logger.h"

#include <algorithm>
#include <iostream>
#include <map>

#include "toolbox.h"

namespace Buzz {

namespace logger {
using namespace rapidjson;

static int64_t CONTAINER_RUNS = 0;

LogEntry NewEntry(const char* msg, util::time::time_point timestamp) {
  return LogEntry(msg, timestamp);
}

void IncrementRunCounter() { CONTAINER_RUNS++; }

LogEntry::LogEntry(const char* msg, util::time::time_point timestamp)
    : buffer_(), writer_(buffer_) {
  writer_.StartObject();
  writer_.Key("ts");
  auto ts =
      std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch())
          .count();
  writer_.Int64(ts);
  writer_.Key("msg");
  writer_.String(msg);
  if (CONTAINER_RUNS > 0) {
    writer_.Key("run");
    writer_.Int64(CONTAINER_RUNS);
  }
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
  std::cout << buffer_.GetString() << std::endl;
}

}  // namespace logger
}  // namespace Buzz