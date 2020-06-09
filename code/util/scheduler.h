#pragma once

#include <condition_variable>
#include <thread>
#include <unordered_set>

#include "arrow/filesystem/filesystem.h"
#include "arrow/status.h"
#include "arrow/util/macros.h"

namespace util {

using namespace arrow;

class ARROW_EXPORT ResourceScheduler {
 public:
  // A scheduler that manages queus for download and processing.
  // TODO: handle dequeue and unsubscription on error.
  ResourceScheduler(int16_t max_concurrent_dl, int16_t max_concurrent_proc);
  // Register this thread as using the download on processing queues of this scheduler. It
  // will be registered until colling NotifyProcessingDone.
  Status RegisterThreadForSync();
  // Check whether the current thread is registered
  Result<bool> IsThreadRegisteredForSync();
  // Queue for a download slot.
  Status WaitDownloadSlot();
  // Notify the scheduler that the download is finished.
  Status NotifyDownloadDone();
  // Queue for a processing slot.
  Status WaitProcessingSlot();
  // Notify that the processing is finished. This un-registers this thread from the
  // scheduler.
  Status NotifyProcessingDone();

 private:
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
};

}  // namespace util