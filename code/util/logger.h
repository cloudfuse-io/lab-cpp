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

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

namespace util {
using namespace rapidjson;
using namespace boost::posix_time;

class LogEntry {
 public:
  void StrField(const char* key, const char* value);
  void IntField(const char* key, int64_t value);
  void FloatField(const char* key, double value);
  void Log();
  friend class Logger;

 private:
  LogEntry(const char* msg, bool is_local);
  StringBuffer buffer_;
  Writer<StringBuffer> writer_;
  bool is_local_;
};

class Logger {
 public:
  Logger(bool is_local);
  LogEntry NewEntry(const char* msg, bool is_local = true) {
    return LogEntry(msg, is_local_);
  }

 private:
  bool is_local_;
};

Logger::Logger(bool is_local) : is_local_(is_local) {
  if (!is_local) {
    // init s3
  }
}

LogEntry::LogEntry(const char* msg, bool is_local)
    : buffer_(), writer_(buffer_), is_local_(is_local) {
  writer_.StartObject();
  writer_.Key("time");
  writer_.String(to_iso_extended_string(microsec_clock::universal_time()).data());
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
    // add to s3 buffer
  }
}
}  // namespace util
