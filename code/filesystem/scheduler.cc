#include "scheduler.h"

#include <unordered_map>

#include "arrow/result.h"
#include "arrow/status.h"
#include "toolbox.h"

namespace arrow {
namespace fs {
namespace fork {

ResourceScheduler::ResourceScheduler(int16_t max_concurrent_dl,
                                     int16_t max_concurrent_proc) {
  max_concurrent_dl_ = max_concurrent_dl;
  max_concurrent_proc_ = max_concurrent_proc;
}

Status ResourceScheduler::RegisterThreadForSync() {
  std::unique_lock<std::mutex> lk(registered_thread_mutex_);
  registered_threads_.insert(std::this_thread::get_id());
  return Status::OK();
}

Result<bool> ResourceScheduler::IsThreadRegisteredForSync() {
  std::unique_lock<std::mutex> lk(registered_thread_mutex_);
  return registered_threads_.find(std::this_thread::get_id()) !=
         registered_threads_.end();
}

Status ResourceScheduler::WaitDownloadSlot() {
  std::unique_lock<std::mutex> lk(concurrent_dl_mutex_);
  concurrent_dl_cv_.wait(lk, [this] { return concurrent_dl_ < max_concurrent_dl_; });
  ++concurrent_dl_;
  return Status::OK();
}

Status ResourceScheduler::NotifyDownloadDone() {
  {
    std::unique_lock<std::mutex> lk(concurrent_dl_mutex_);
    if (concurrent_dl_ > 0) {
      --concurrent_dl_;
    }
  }
  concurrent_dl_cv_.notify_one();
  return Status::OK();
}

Status ResourceScheduler::WaitProcessingSlot() {
  std::unique_lock<std::mutex> lk(concurrent_proc_mutex_);
  concurrent_proc_cv_.wait(lk,
                           [this] { return concurrent_proc_ < max_concurrent_proc_; });
  ++concurrent_proc_;
  return Status::OK();
}

Status ResourceScheduler::NotifyProcessingDone() {
  {
    std::unique_lock<std::mutex> lk(registered_thread_mutex_);
    registered_threads_.erase(std::this_thread::get_id());
  }
  {
    std::unique_lock<std::mutex> lk(concurrent_proc_mutex_);
    if (concurrent_proc_ > 0) {
      --concurrent_proc_;
    }
  }
  concurrent_proc_cv_.notify_one();
  return Status::OK();
}

}  // namespace fork
}  // namespace fs
}  // namespace arrow