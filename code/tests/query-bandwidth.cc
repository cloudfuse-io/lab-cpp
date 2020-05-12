
#include <arrow/api.h>
#include "s3fs.h"
#include <arrow/compute/api.h>
#include <arrow/io/api.h>
#include <aws/lambda-runtime/runtime.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>
#include <future>

#include <iostream>

std::mutex g_lock;

int64_t download_chunck(std::shared_ptr<arrow::io::RandomAccessFile> infile, int64_t start, int64_t nbbytes) {
  g_lock.lock();
  g_lock.unlock();
  auto start_time = std::chrono::high_resolution_clock::now();
  std::shared_ptr<arrow::Buffer> buf;
  PARQUET_ASSIGN_OR_THROW(buf, infile->ReadAt(start, nbbytes));
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  return duration;
}

static aws::lambda_runtime::invocation_response my_handler(
    std::shared_ptr<arrow::fs::S3FileSystem> fs,
    aws::lambda_runtime::invocation_request const &req
  )
{
  std::vector<std::string> file_names{
    "bb-test-data-dev/bid-large.parquet",
    "bb-test-data-dev/bid-large-bis.parquet",
  };

  int64_t chunck_size = 250000;
  for (auto file_name: file_names) {
    
    int nb_parallel = 4;
    std::shared_ptr<arrow::io::RandomAccessFile> infile;
    PARQUET_ASSIGN_OR_THROW(
      infile, fs->OpenInputFile(file_name));
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::future<int64_t>> dl_futures;
    dl_futures.reserve(nb_parallel);
    g_lock.lock();
    for (int i=0; i<nb_parallel; i++) {
      auto fut = std::async(std::launch::async, download_chunck, infile, i*chunck_size, chunck_size);
      dl_futures.push_back(std::move(fut));
    }
    g_lock.unlock();
    std::cout << "=> parallel:" << nb_parallel << std::endl;
    std::cout << "speed:";
    int64_t added_speed = 0;
    for (auto& dl_future: dl_futures) {
      auto local_duration = dl_future.get();
      auto local_speed = chunck_size/local_duration;
      added_speed+=local_speed;
      // std::cout << local_speed << ",";
    }
    std::cout << std::endl;
    // std::cout << "added_speed:" << added_speed << std::endl;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    std::cout << "dl_duration_ms:" << total_duration/1000 << std::endl;
    std::cout << "true_speed:" << chunck_size*nb_parallel/total_duration << std::endl;
  }

    // std::vector<std::pair<int64_t,int64_t>> metrics;
    // PARQUET_ASSIGN_OR_THROW(metrics, fs->Metrics());
    // std::cout << "metrics: ";
    // std::copy(metrics.begin(), metrics.end(), std::ostream_iterator<int>(std::cout, ","));
    // std::cout << std::endl;

  return aws::lambda_runtime::invocation_response::success("Yessss!", "text/plain");
}

int main()
{
  // init SDK
  arrow::fs::S3GlobalOptions global_options;
  global_options.log_level = arrow::fs::S3LogLevel::Warn;
  PARQUET_THROW_NOT_OK(InitializeS3(global_options));
  // init s3 client
  arrow::fs::S3Options options = arrow::fs::S3Options::Defaults();
  options.region = "eu-west-1";
  bool is_local = getenv("IS_LOCAL") != NULL && strcmp(getenv("IS_LOCAL"), "true") == 0;
  if (is_local)
  {
    options.endpoint_override = "minio:9000";
    std::cout << "endpoint_override=" << options.endpoint_override << std::endl;
    options.scheme = "http";
  }
  std::shared_ptr<arrow::fs::S3FileSystem> fs;
  PARQUET_ASSIGN_OR_THROW(fs, arrow::fs::S3FileSystem::Make(options));
  auto handler_lambda = [fs] (aws::lambda_runtime::invocation_request const & req) { return my_handler(fs, req); };
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


  // int64_t chunck_size = 250000;
  // auto already_scanned = 0;
  // for (int nb_parallel = 15; nb_parallel >= 13; nb_parallel-=2) {
  //   auto start_time = std::chrono::high_resolution_clock::now();
  //   std::vector<std::future<int64_t>> dl_futures;
  //   dl_futures.reserve(nb_parallel);
  //   for (int i=0; i<nb_parallel; i++) {
  //     auto fut = std::async(std::launch::async, download_chunck, infile, (already_scanned+i)*chunck_size, chunck_size);
  //     dl_futures.push_back(std::move(fut));
  //   }
  //   std::cout << "=> parallel:" << nb_parallel << std::endl;
  //   std::cout << "speed:";
  //   int64_t added_speed = 0;
  //   for (auto& dl_future: dl_futures) {
  //     auto local_duration = dl_future.get();
  //     auto local_speed = chunck_size/local_duration;
  //     added_speed+=local_speed;
  //     std::cout << local_speed << ",";
  //   }
  //   std::cout << std::endl;
  //   // std::cout << "added_speed:" << added_speed << std::endl;
  //   auto end_time = std::chrono::high_resolution_clock::now();
  //   auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  //   std::cout << "true_speed:" << chunck_size*nb_parallel/total_duration << std::endl;

  //   already_scanned += nb_parallel;
  // }



  // std::vector<std::future<void>> del_futures;
  // del_futures.reserve(16);
  // for (int nb_parallel = 1; nb_parallel <= 16; nb_parallel++) {
  //   auto delete_res = std::async(std::launch::async, [fs]{ fs->DeleteFile("bb-test-data-dev/fake-file"); });
  //   del_futures.push_back(std::move(delete_res));
  // }

  // for (auto& del_future: del_futures) {
  //   std::cout << del_future.valid();
  // }
  // std::cout << std::endl;