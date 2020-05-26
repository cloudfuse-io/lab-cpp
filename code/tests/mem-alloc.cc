
#include <arrow/api.h>
#include <arrow/filesystem/api.h>
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>
#include <future>

#include <iostream>

int64_t get_duration(std::chrono::_V2::system_clock::time_point start, std::chrono::_V2::system_clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
}

int64_t GetEnvInt(const char *name, int64_t def) {
  auto raw_var = getenv(name);
  if (raw_var == nullptr) {
    return def;
  }
  return std::stoi(raw_var);
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const &req
  )
{
  std::cout << "backend_name:" << arrow::default_memory_pool()->backend_name() << std::endl;
  std::cout << "GROWING IN TWO PHASES 2MB" << std::endl;

  for (int i=1; i<=10; i++) {
    std::cout << i << ",";
    std::cout << arrow::default_memory_pool()->bytes_allocated() << ",";
    auto start_time = std::chrono::high_resolution_clock::now();
    {
      std::vector<std::shared_ptr<arrow::Buffer>> buffers;
      for (int j=0; j<GetEnvInt("MEGA_ALLOCATED",1000)/2; j++) {
        std::shared_ptr<arrow::Buffer> buf;
        PARQUET_ASSIGN_OR_THROW(buf, arrow::AllocateBuffer(2*1024*1024, arrow::default_memory_pool()));
        memset(buf->mutable_data(), i, static_cast<size_t>(buf->size()));
        buffers.push_back(buf);
      }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << get_duration(start_time,end_time) << ",";
    std::cout << arrow::default_memory_pool()->bytes_allocated() << ",";
    std::cout << arrow::default_memory_pool()->max_memory() << std::endl;
  }
  for (int i=1; i<=10; i++) {
    std::cout << i << ",";
    std::cout << arrow::default_memory_pool()->bytes_allocated() << ",";
    auto start_time = std::chrono::high_resolution_clock::now();
    {
      std::vector<std::shared_ptr<arrow::Buffer>> buffers;
      for (int j=0; j<GetEnvInt("MEGA_ALLOCATED",1000)*2/2; j++) {
        std::shared_ptr<arrow::Buffer> buf;
        PARQUET_ASSIGN_OR_THROW(buf, arrow::AllocateBuffer(2*1024*1024, arrow::default_memory_pool()));
        memset(buf->mutable_data(), i, static_cast<size_t>(buf->size()));
        buffers.push_back(buf);
      }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << get_duration(start_time,end_time) << ",";
    std::cout << arrow::default_memory_pool()->bytes_allocated() << ",";
    std::cout << arrow::default_memory_pool()->max_memory() << std::endl;
  }

  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

int main()
{
  bool is_local = getenv("IS_LOCAL") != NULL && strcmp(getenv("IS_LOCAL"), "true") == 0;
  auto handler_lambda = [] (aws::lambda_runtime::invocation_request const & req) { return my_handler(req); };
  if (is_local)
  {
    aws::lambda_runtime::invocation_response response = handler_lambda(aws::lambda_runtime::invocation_request());
    std::cout << response.get_payload() << std::endl;
  }
  else
  {
    aws::lambda_runtime::run_handler(handler_lambda);
  }
  return 0;
}
