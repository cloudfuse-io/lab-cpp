
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>

#include <future>
#include <iostream>
#include <unordered_set>

#include "various.h"

constexpr int64_t PAGE_SIZE = 4 * 1024;
constexpr int64_t NB_REPETITION = 2;

static int64_t nb_allocations = util::getenv_int("NB_ALLOCATION", 100);
static int64_t allocation_size_byte =
    util::getenv_int("ALLOCATION_SIZE_BYTE", 1024 * 1024);

// the max nb of pages that we expect to allocate
static size_t max_pages_est =
    nb_allocations * allocation_size_byte / PAGE_SIZE * NB_REPETITION;
static std::unordered_set<uint32_t> allocated_pages(max_pages_est);

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req) {
  std::cout << "backend_name:" << arrow::default_memory_pool()->backend_name()
            << std::endl;
  std::cout << "ALLOCATING " << nb_allocations << " CHUNCKS OF " << allocation_size_byte
            << " BYTES" << std::endl;
  std::cout << "iteration;allocation_duration;new_pages_allocated" << std::endl;

  for (int i = 1; i <= NB_REPETITION; i++) {
    std::vector<std::shared_ptr<arrow::Buffer>> buffers;
    for (int j = 0; j < nb_allocations; j++) {
      uint32_t new_pages_allocated{0};
      std::shared_ptr<arrow::Buffer> buf;
      auto start_time = std::chrono::high_resolution_clock::now();
      int64_t size_to_allocate = allocation_size_byte;
      PARQUET_ASSIGN_OR_THROW(
          buf, arrow::AllocateBuffer(size_to_allocate, arrow::default_memory_pool()));
      memset(buf->mutable_data(), i, static_cast<size_t>(buf->size()));
      auto end_time = std::chrono::high_resolution_clock::now();
      auto current_ptr_page = buf->mutable_address() / PAGE_SIZE;
      for (int i = 0; i < size_to_allocate / PAGE_SIZE; i++) {
        if (allocated_pages.insert(current_ptr_page + i).second) {
          new_pages_allocated++;
        }
      }
      buffers.push_back(buf);

      std::cout << i << ";";
      std::cout << util::get_duration_micro(start_time, end_time) << ";";
      std::cout << new_pages_allocated << std::endl;
    }
  }
  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

int main() {
  bool is_local = getenv("IS_LOCAL") != NULL && strcmp(getenv("IS_LOCAL"), "true") == 0;
  auto handler_lambda = [](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(req);
  };
  if (is_local) {
    aws::lambda_runtime::invocation_response response =
        handler_lambda(aws::lambda_runtime::invocation_request());
    std::cout << response.get_payload() << std::endl;
  } else {
    aws::lambda_runtime::run_handler(handler_lambda);
  }
  return 0;
}
