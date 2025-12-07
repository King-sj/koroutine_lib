#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "executor.h"
#include "koroutine/debug.h"

namespace koroutine {

/**
 * @brief A multi-threaded executor with a thread pool and delayed task support.
 *
 * Features:
 * - Fixed size thread pool for immediate task execution.
 * - Dedicated timer thread for handling delayed tasks efficiently.
 * - Graceful shutdown mechanism.
 * - Thread-safe task submission.
 */
class ThreadPoolExecutor : public AbstractExecutor {
 public:
  /**
   * @brief Construct a new Thread Pool Executor
   *
   * @param threads Number of worker threads. Defaults to hardware concurrency.
   */
  explicit ThreadPoolExecutor(
      size_t threads = std::thread::hardware_concurrency())
      : stop_(false) {
    if (threads == 0) threads = 1;

    LOG_INFO("ThreadPoolExecutor: Starting with ", threads, " threads");

    // Start worker threads
    for (size_t i = 0; i < threads; ++i) {
      workers_.emplace_back([this, i] {
        (void)i;  // Suppress unused warning if logging is disabled
        LOG_TRACE("ThreadPoolExecutor: Worker ", i, " started");
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            this->condition_.wait(
                lock, [this] { return this->stop_ || !this->tasks_.empty(); });

            if (this->stop_ && this->tasks_.empty()) {
              LOG_TRACE("ThreadPoolExecutor: Worker ", i, " stopping");
              return;
            }

            task = std::move(this->tasks_.front());
            this->tasks_.pop();
          }
          try {
            task();
          } catch (const std::exception& e) {
            LOG_ERROR("ThreadPoolExecutor: Task threw exception: ", e.what());
          } catch (...) {
            LOG_ERROR("ThreadPoolExecutor: Task threw unknown exception");
          }
        }
      });
    }

    // Start timer thread for delayed tasks
    timer_thread_ = std::thread([this] {
      LOG_TRACE("ThreadPoolExecutor: Timer thread started");
      while (true) {
        std::unique_lock<std::mutex> lock(timer_mutex_);

        if (stop_ && delayed_tasks_.empty()) {
          LOG_TRACE("ThreadPoolExecutor: Timer thread stopping");
          return;
        }

        if (delayed_tasks_.empty()) {
          timer_cv_.wait(lock,
                         [this] { return stop_ || !delayed_tasks_.empty(); });
          if (stop_ && delayed_tasks_.empty()) return;
        }

        // Check top task
        auto now = std::chrono::steady_clock::now();
        // Use const_cast to move the function out before popping, as top()
        // returns const ref
        if (delayed_tasks_.top().first <= now) {
          auto task = std::move(
              const_cast<std::function<void()>&>(delayed_tasks_.top().second));
          delayed_tasks_.pop();
          lock.unlock();

          // Submit to main thread pool
          execute(std::move(task));
        } else {
          auto next_time = delayed_tasks_.top().first;
          timer_cv_.wait_until(lock, next_time);
        }
      }
    });
  }

  ~ThreadPoolExecutor() override { shutdown(); }

  void execute(std::function<void()>&& func) override {
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      if (stop_) {
        LOG_WARN("ThreadPoolExecutor: execute called on stopped executor");
        return;
        // Alternatively throw, but logging is safer for destructors
      }
      tasks_.emplace(std::move(func));
    }
    condition_.notify_one();
  }

  void execute_delayed(std::function<void()>&& func, long long ms) override {
    auto execute_at =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    {
      std::unique_lock<std::mutex> lock(timer_mutex_);
      if (stop_) {
        LOG_WARN(
            "ThreadPoolExecutor: execute_delayed called on stopped executor");
        return;
      }
      delayed_tasks_.push({execute_at, std::move(func)});
    }
    // Notify timer thread to re-evaluate wait time (in case this task is
    // sooner than current top)
    timer_cv_.notify_one();
  }

  void shutdown() {
    if (stop_.exchange(true)) return;  // Already stopped

    LOG_INFO("ThreadPoolExecutor: Shutting down...");

    // Wake up all workers
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      // stop_ is already true
    }
    condition_.notify_all();

    // Wake up timer thread
    {
      std::unique_lock<std::mutex> lock(timer_mutex_);
      // stop_ is already true
    }
    timer_cv_.notify_all();

    for (std::thread& worker : workers_) {
      if (worker.joinable()) worker.join();
    }
    if (timer_thread_.joinable()) timer_thread_.join();

    LOG_INFO("ThreadPoolExecutor: Shutdown complete");
  }

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;

  std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::atomic<bool> stop_;

  // Timer related
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using TaskPair = std::pair<TimePoint, std::function<void()>>;

  struct CompareTasks {
    bool operator()(const TaskPair& a, const TaskPair& b) const {
      return a.first > b.first;  // Min heap based on time
    }
  };

  std::priority_queue<TaskPair, std::vector<TaskPair>, CompareTasks>
      delayed_tasks_;
  std::mutex timer_mutex_;
  std::condition_variable timer_cv_;
  std::thread timer_thread_;
};

}  // namespace koroutine
