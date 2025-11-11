#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

#include "executor.h"

namespace koroutine {

class LooperExecutor : public AbstractExecutor {
 private:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using TaskPair = std::pair<TimePoint, std::function<void()>>;

  struct CompareTasks {
    bool operator()(const TaskPair& a, const TaskPair& b) const {
      return a.first > b.first;
    }
  };

  std::priority_queue<TaskPair, std::vector<TaskPair>, CompareTasks>
      delayed_tasks_;
  std::queue<std::function<void()>> tasks_;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> is_active_{true};
  std::thread worker_;

  void run_loop() {
    while (is_active_ || !tasks_.empty() || !delayed_tasks_.empty()) {
      LOG_TRACE("LooperExecutor::run_loop - waiting for tasks");
      std::unique_lock lock(mutex_);
      LOG_TRACE("LooperExecutor::run_loop - acquired lock");

      if (tasks_.empty() && delayed_tasks_.empty()) {
        cv_.wait(lock);
        if (!is_active_ && tasks_.empty() && delayed_tasks_.empty()) break;
      }

      if (tasks_.empty()) {
        auto wait_time = delayed_tasks_.top().first - Clock::now();
        if (wait_time > Clock::duration::zero()) {
          cv_.wait_for(lock, wait_time);
        }
      }

      // Move ready delayed tasks to the immediate queue
      auto now = Clock::now();
      while (!delayed_tasks_.empty() && delayed_tasks_.top().first <= now) {
        LOG_TRACE(
            "LooperExecutor::run_loop - moving delayed task to immediate "
            "queue");
        tasks_.push(
            std::move(const_cast<TaskPair&>(delayed_tasks_.top()).second));
        delayed_tasks_.pop();
      }

      // Execute immediate tasks
      if (!tasks_.empty()) {
        LOG_TRACE("LooperExecutor::run_loop - executing immediate task");
        auto task = std::move(tasks_.front());
        tasks_.pop();
        lock.unlock();
        task();
      }
    }
  }

 public:
  LooperExecutor() : worker_(&LooperExecutor::run_loop, this) {}

  ~LooperExecutor() {
    shutdown();
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  void execute(std::function<void()>&& func) override {
    LOG_TRACE("LooperExecutor::execute - adding task to queue");
    {
      std::unique_lock lock(mutex_);
      LOG_DEBUG("LooperExecutor::execute called.");
      if (is_active_) {
        tasks_.push(std::move(func));
      }
    }
    cv_.notify_one();
  }

  void execute_delayed(std::function<void()>&& func, long long ms) override {
    LOG_TRACE(
        "LooperExecutor::execute_delayed - adding delayed task to queue with "
        "delay: ",
        ms);
    {
      std::unique_lock lock(mutex_);
      LOG_DEBUG("LooperExecutor::execute_delayed called.", ms);
      if (is_active_) {
        auto time_point = Clock::now() + std::chrono::milliseconds(ms);
        delayed_tasks_.push({time_point, std::move(func)});
      }
    }
    cv_.notify_one();
  }

  void shutdown() {
    LOG_TRACE("LooperExecutor::shutdown - shutting down executor");
    is_active_ = false;
    cv_.notify_all();
  }

  std::thread::id get_thread_id() const { return worker_.get_id(); }
};

}  // namespace koroutine