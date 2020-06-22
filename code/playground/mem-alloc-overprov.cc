
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

// #include "jemalloc_ep/dist/include/jemalloc/jemalloc.h"
#include "toolbox.h"

extern char* je_arrow_malloc_conf;
constexpr int64_t PAGE_SIZE = 4 * 1024;
int64_t NB_REPETITION = util::getenv_int("NB_REPETITION", 100);

static int64_t NB_ALLOCATION = util::getenv_int("NB_ALLOCATION", 100);
static int64_t ALLOCATION_SIZE_BYTE =
    util::getenv_int("ALLOCATION_SIZE_BYTE", 1024 * 1024);

// the max nb of pages that we expect to allocate
static size_t max_pages_est =
    NB_ALLOCATION * ALLOCATION_SIZE_BYTE / PAGE_SIZE * NB_REPETITION;
static std::unordered_set<uint32_t> allocated_pages(max_pages_est);

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req) {
  std::cout << "je_arrow_malloc_conf:" << je_arrow_malloc_conf << std::endl;
  std::cout << "backend_name:" << arrow::default_memory_pool()->backend_name()
            << std::endl;
  std::cout << "ALLOCATING BETWEEN " << NB_ALLOCATION << " AND "
            << NB_ALLOCATION + NB_REPETITION << " CHUNCKS OF " << ALLOCATION_SIZE_BYTE
            << " BYTES" << std::endl;

  for (int i = 1; i <= NB_REPETITION; i++) {
    std::vector<std::shared_ptr<arrow::Buffer>> buffers;
    for (int j = 0; j < NB_ALLOCATION + i; j++) {
      uint32_t new_pages_allocated{0};
      std::shared_ptr<arrow::Buffer> buf;
      PARQUET_ASSIGN_OR_THROW(
          buf, arrow::AllocateBuffer(ALLOCATION_SIZE_BYTE, arrow::default_memory_pool()));
      memset(buf->mutable_data(), i, static_cast<size_t>(buf->size()));

      auto current_ptr_page = buf->mutable_address() / PAGE_SIZE;
      for (int i = 0; i < ALLOCATION_SIZE_BYTE / PAGE_SIZE; i++) {
        if (allocated_pages.insert(current_ptr_page + i).second) {
          new_pages_allocated++;
        }
      }
      buffers.push_back(buf);
    }
    std::cout << allocated_pages.size() * PAGE_SIZE / (1024 * 1024) << " ";
  }
  std::cout << std::endl;
  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

int main() {
  auto handler_lambda = [](aws::lambda_runtime::invocation_request const& req) {
    return my_handler(req);
  };
  if (util::getenv_bool("IS_LOCAL", false)) {
    aws::lambda_runtime::invocation_response response =
        handler_lambda(aws::lambda_runtime::invocation_request());
    std::cout << response.get_payload() << std::endl;
  } else {
    aws::lambda_runtime::run_handler(handler_lambda);
  }
  return 0;
}
