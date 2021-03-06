#pragma once

#include <arrow/result.h>
#include <arrow/status.h>

namespace Buzz {
using StatusCode = arrow::StatusCode;

using Status = arrow::Status;

template <typename T>
using Result = arrow::Result<T>;
}  // namespace Buzz