#include "scheduler.h"

#include <arrow/result.h>
#include <arrow/status.h>

#include <unordered_map>

#include "toolbox.h"

namespace util {

using namespace arrow;

class ResourceScheduler::Impl {
 public:
  std::mutex registered_thread_mutex_;
  std::unordered_set<std::thread::id> registered_threads_;
  std::mutex concurrent_dl_mutex_;
  std::condition_variable concurrent_dl_cv_;
  int16_t concurrent_dl_ = 0;
  int16_t max_concurrent_dl_ = 1;
  std::mutex concurrent_proc_mutex_;
  std::condition_variable concurrent_proc_cv_;
  int16_t concurrent_proc_ = 0;
  int16_t max_concurrent_proc_ = 1;

  Impl(int16_t max_concurrent_dl, int16_t max_concurrent_proc)
      : max_concurrent_dl_(max_concurrent_dl),
        max_concurrent_proc_(max_concurrent_proc) {}

  Status RegisterThreadForSync() {
    std::lock_guard<std::mutex> lk(registered_thread_mutex_);
    registered_threads_.insert(std::this_thread::get_id());
    return Status::OK();
  }

  Result<bool> IsThreadRegisteredForSync() {
    std::lock_guard<std::mutex> lk(registered_thread_mutex_);
    return registered_threads_.find(std::this_thread::get_id()) !=
           registered_threads_.end();
  }

  Status WaitDownloadSlot() {
    std::unique_lock<std::mutex> lk(concurrent_dl_mutex_);
    concurrent_dl_cv_.wait(lk, [this] { return concurrent_dl_ < max_concurrent_dl_; });
    ++concurrent_dl_;
    return Status::OK();
  }

  Status NotifyDownloadDone() {
    {
      std::lock_guard<std::mutex> lk(concurrent_dl_mutex_);
      if (concurrent_dl_ > 0) {
        --concurrent_dl_;
      }
    }
    concurrent_dl_cv_.notify_one();
    return Status::OK();
  }

  Status WaitProcessingSlot() {
    std::unique_lock<std::mutex> lk(concurrent_proc_mutex_);
    concurrent_proc_cv_.wait(lk,
                             [this] { return concurrent_proc_ < max_concurrent_proc_; });
    ++concurrent_proc_;
    return Status::OK();
  }

  Status NotifyProcessingDone() {
    {
      std::lock_guard<std::mutex> lk(registered_thread_mutex_);
      registered_threads_.erase(std::this_thread::get_id());
    }
    {
      std::lock_guard<std::mutex> lk(concurrent_proc_mutex_);
      if (concurrent_proc_ > 0) {
        --concurrent_proc_;
      }
    }
    concurrent_proc_cv_.notify_one();
    return Status::OK();
  }
};

ResourceScheduler::ResourceScheduler(int16_t max_concurrent_dl,
                                     int16_t max_concurrent_proc)
    : impl_{new Impl(max_concurrent_dl, max_concurrent_proc)} {}

ResourceScheduler::~ResourceScheduler() {}

Status ResourceScheduler::RegisterThreadForSync() {
  return impl_->RegisterThreadForSync();
}

Result<bool> ResourceScheduler::IsThreadRegisteredForSync() {
  return impl_->IsThreadRegisteredForSync();
}

Status ResourceScheduler::WaitDownloadSlot() { return impl_->WaitDownloadSlot(); }

Status ResourceScheduler::NotifyDownloadDone() { return impl_->NotifyDownloadDone(); }

Status ResourceScheduler::WaitProcessingSlot() { return impl_->WaitProcessingSlot(); }

Status ResourceScheduler::NotifyProcessingDone() { return impl_->NotifyProcessingDone(); }

}  // namespace util