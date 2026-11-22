#pragma once

// Core-thread task queue shared by the macOS HAL backends. HAL threads (libusb
// and CoreAudio callbacks) post; the core calls drain() on its own thread, so
// every D1 sink runs on the core thread (threading decision in D1).

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>

namespace hu::hal::macos {

class CoreQueue {
 public:
  using Task = std::function<void()>;

  // Any thread.
  void post(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
  }

  // Core thread only. Runs tasks posted before the call, in order. Tasks posted while draining wait for the next call.
  size_t drain() {
    std::deque<Task> batch;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      batch.swap(tasks_);
    }
    for (auto& task : batch) task();
    return batch.size();
  }

 private:
  std::mutex mutex_;
  std::deque<Task> tasks_;
};

}  // namespace hu::hal::macos
