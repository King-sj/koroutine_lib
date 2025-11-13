#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "delayed_executable.h"

namespace koroutine {
/**
 * @brief 定时任务调度器
 */
class TimerScheduler {
  std::condition_variable queue_condition;
  std::mutex queue_lock;
  std::priority_queue<DelayedExecutable, std::vector<DelayedExecutable>,
                      DelayedExecutableCompare>
      executable_queue;

  std::atomic<bool> is_active;
  std::thread work_thread;

  void run_loop() {
    while (is_active.load(std::memory_order_relaxed) ||
           !executable_queue.empty()) {
      std::unique_lock lock(queue_lock);
      if (executable_queue.empty()) {
        queue_condition.wait(lock);
        if (executable_queue.empty()) {
          continue;
        }
      }
      auto executable = executable_queue.top();
      long long delay = executable.delay();
      if (delay > 0) {
        auto status =
            queue_condition.wait_for(lock, std::chrono::milliseconds(delay));
        if (status != std::cv_status::timeout) {
          // a new executable should be executed before.
          continue;
        }
      }
      executable_queue.pop();
      lock.unlock();
      executable();
    }
    LOG_DEBUG("run_loop exit.");
  }

 public:
  TimerScheduler() {
    is_active.store(true, std::memory_order_relaxed);
    work_thread = std::thread(&TimerScheduler::run_loop, this);
  }
  ~TimerScheduler() {
    shutdown(false);
    join();
  }

  void schedule(std::function<void()>&& func, long long delay = 0) {
    delay = delay < 0 ? 0 : delay;
    std::unique_lock lock(queue_lock);
    if (is_active.load(std::memory_order_relaxed)) {
      bool need_notify =
          executable_queue.empty() || delay < executable_queue.top().delay();

      executable_queue.push(DelayedExecutable(std::move(func), delay));
      lock.unlock();
      if (need_notify) {
        queue_condition.notify_one();
      }
    }
  }
  void shutdown(bool wait_for_empty = true) {
    is_active.store(false, std::memory_order_relaxed);
    if (!wait_for_empty) {
      // clear the queue
      std::unique_lock lock(queue_lock);
      decltype(executable_queue) empty_queue;
      std::swap(executable_queue, empty_queue);
      lock.unlock();
    }
    queue_condition.notify_all();
  }
  void join() {
    if (work_thread.joinable()) {
      work_thread.join();
    }
  }
};
}  // namespace koroutine