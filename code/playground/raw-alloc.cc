#include <aws/lambda-runtime/runtime.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <map>
#include <thread>

#include "toolbox.h"

static int64_t NB_PAGE = util::getenv_int("NB_PAGE", 1024);
static std::string ALLOC_TEST_NAME = util::getenv("ALLOC_TEST_NAME", "mmap_hugepage");

void mmap_munmap_inplace() {
  size_t pagesize = getpagesize();
  int64_t allocation_size = NB_PAGE * pagesize;
  int64_t run_number = 0;
  while (run_number < 100) {
    run_number++;
    void* region_raw_ptr =
        mmap((void*)(pagesize * (1 << 20)),  // Map from the start of the 2^20th page
             allocation_size,                // for one page length
             PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_ANON | MAP_PRIVATE,  // to a private block of hardware memory
             0, 0);
    if (region_raw_ptr == MAP_FAILED) {
      perror("Could not mmap");
      return;
    } else {
      std::cout << NB_PAGE << " mmaped pages at page "
                << reinterpret_cast<uintptr_t>(region_raw_ptr) / pagesize << std::endl;
    }

    int paint = run_number;
    memset(region_raw_ptr, paint, static_cast<size_t>(allocation_size));

    int unmap_result = munmap(region_raw_ptr, allocation_size);
    if (unmap_result != 0) {
      perror("Could not munmap");
      return;
    }
  }
}
void mmap_munmap_newplace() {
  size_t pagesize = getpagesize();
  int64_t allocation_size = NB_PAGE * pagesize;
  int64_t run_number = 0;
  while (run_number < 100) {
    void* region_raw_ptr =
        mmap((void*)(pagesize * (1 << 20) +
                     allocation_size *
                         (run_number++)),  // Map from the start of the 2^20th page
             allocation_size,              // for one page length
             PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_ANON | MAP_PRIVATE,  // to a private block of hardware memory
             0, 0);
    if (region_raw_ptr == MAP_FAILED) {
      perror("Could not mmap");
      return;
    } else {
      std::cout << NB_PAGE << " mmaped pages at page "
                << reinterpret_cast<uintptr_t>(region_raw_ptr) / pagesize << std::endl;
    }

    int paint = run_number;
    memset(region_raw_ptr, paint, static_cast<size_t>(allocation_size));

    int unmap_result = munmap(region_raw_ptr, allocation_size);
    if (unmap_result != 0) {
      perror("Could not munmap");
      return;
    }
  }
}
void mmap_madv_inplace() {
  size_t pagesize = getpagesize();
  int64_t allocation_size = NB_PAGE * pagesize;
  void* region_raw_ptr =
      mmap((void*)(pagesize * (1 << 20)),  // Map from the start of the 2^20th page
           allocation_size,                // for one page length
           PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_ANON | MAP_PRIVATE,  // to a private block of hardware memory
           0, 0);
  if (region_raw_ptr == MAP_FAILED) {
    perror("Could not mmap");
    return;
  } else {
    std::cout << NB_PAGE << " mmaped pages at page "
              << reinterpret_cast<uintptr_t>(region_raw_ptr) / pagesize << std::endl;
  }

  int64_t run_number = 0;
  while (run_number < 100) {
    run_number++;
    int paint = run_number;
    std::cout << "painting pages with: " << paint << std::endl;
    memset(region_raw_ptr, paint, static_cast<size_t>(allocation_size));
    int madv_result = madvise(region_raw_ptr, allocation_size, MADV_DONTNEED);
    if (madv_result != 0) {
      perror("Could not madvise");
      return;
    }
  }
}
void mmap_madv_newplace() {
  size_t pagesize = getpagesize();
  int64_t allocation_size = NB_PAGE * pagesize;
  int64_t run_number = 0;
  while (run_number < 100) {
    void* region_raw_ptr =
        mmap((void*)(pagesize * (1 << 20) +
                     allocation_size *
                         (run_number++)),  // Map from the start of the 2^20th page
             allocation_size,              // for one page length
             PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_ANON | MAP_PRIVATE,  // to a private block of hardware memory
             0, 0);
    if (region_raw_ptr == MAP_FAILED) {
      perror("Could not mmap");
      return;
    } else {
      std::cout << NB_PAGE << " mmaped pages at page "
                << reinterpret_cast<uintptr_t>(region_raw_ptr) / pagesize << std::endl;
    }

    int paint = run_number;
    memset(region_raw_ptr, paint, static_cast<size_t>(allocation_size));

    int madv_result = madvise(region_raw_ptr, allocation_size, MADV_DONTNEED);
    if (madv_result != 0) {
      perror("Could not madvise");
      return;
    }
  }
}
void mmap_not_painted() {
  size_t pagesize = getpagesize();
  int64_t allocation_size = 1024 * 1024 * 1024;
  int64_t nb_allocation = 100;
  void* allocations[nb_allocation];
  for (int alloc_index = 0; alloc_index < nb_allocation; alloc_index++) {
    void* region_raw_ptr =
        mmap(nullptr,  // position auto attributed
             allocation_size, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_ANON | MAP_PRIVATE,  // to a private block of hardware memory
             0, 0);
    if (region_raw_ptr == MAP_FAILED) {
      perror("Could not mmap");
      return;
    }
    std::cout << allocation_size << " bytes mmaped at page "
              << reinterpret_cast<uintptr_t>(region_raw_ptr) / pagesize << std::endl;
    allocations[alloc_index] = region_raw_ptr;
  }
  for (int alloc_index = 0; alloc_index < nb_allocation; alloc_index++) {
    munmap(allocations[alloc_index], allocation_size);
  }
}
void mmap_hugepage() {
  size_t pagesize = getpagesize();
  int64_t allocation_size = 10 * 2 * 1024 * 1024;
  void* region_raw_ptr =
      mmap(nullptr,          // Map from the start of the 2^20th page
           allocation_size,  // for one page length
           PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_ANON | MAP_PRIVATE | MAP_HUGETLB,  // to a private block of hardware memory
           0, 0);
  if (region_raw_ptr == MAP_FAILED) {
    perror("Could not mmap");
    return;
  } else {
    std::cout << NB_PAGE << " mmaped pages at page "
              << reinterpret_cast<uintptr_t>(region_raw_ptr) / pagesize << std::endl;
  }

  int paint = 1;
  memset(region_raw_ptr, paint, static_cast<size_t>(allocation_size));

  int unmap_result = munmap(region_raw_ptr, allocation_size);
  if (unmap_result != 0) {
    perror("Could not munmap");
    return;
  }
}

static aws::lambda_runtime::invocation_response my_handler(
    aws::lambda_runtime::invocation_request const& req) {
  // std::cout << "==> ADDRESS SPACE" << std::endl;
  // std::ifstream maps("/proc/self/maps");
  // char buffer[1024];
  // maps.rdbuf()->sgetn(buffer, 1024);
  // std::cout << buffer << std::endl;

  std::cout << "==> LINUX VERSION" << std::endl;
  struct utsname buf;
  uname(&buf);
  std::cout << buf.sysname << " release:" << buf.release << " version:" << buf.version
            << std::endl;

  std::cout << "==> ALLOC TESTS" << std::endl;
  std::cout << "Alloc test " << ALLOC_TEST_NAME << std::endl;
  typedef void (*pfunc)();
  std::map<std::string, pfunc> func_map;
  func_map["mmap_munmap_inplace"] = mmap_munmap_inplace;
  func_map["mmap_munmap_newplace"] = mmap_munmap_newplace;
  func_map["mmap_madv_inplace"] = mmap_madv_inplace;
  func_map["mmap_madv_newplace"] = mmap_madv_newplace;
  func_map["mmap_not_painted"] = mmap_not_painted;
  func_map["mmap_hugepage"] = mmap_hugepage;
  (*(func_map[ALLOC_TEST_NAME]))();

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
