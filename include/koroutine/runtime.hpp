#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "coroutine_common.h"
#include "scheduler_manager.h"
#include "schedulers/scheduler.h"
#include "task.hpp"
namespace koroutine {

/**
 * @brief 聚合异常，用于在一个异常对象中包含多个异常
 */
class AggregateException : public std::exception {
 public:
  explicit AggregateException(std::vector<std::exception_ptr>&& exceptions)
      : exceptions_(std::move(exceptions)) {
    // 构建合并的 message 字符串
    std::ostringstream oss;
    oss << "AggregateException: " << exceptions_.size() << " exceptions:\n";
    for (size_t i = 0; i < exceptions_.size(); ++i) {
      try {
        std::rethrow_exception(exceptions_[i]);
      } catch (const std::exception& e) {
        oss << "  [" << i << "] std::exception: " << e.what() << '\n';
      } catch (...) {
        oss << "  [" << i << "] unknown exception" << '\n';
      }
    }
    message_ = oss.str();
  }

  const char* what() const noexcept override { return message_.c_str(); }

  const std::vector<std::exception_ptr>& exceptions() const noexcept {
    return exceptions_;
  }

 private:
  std::vector<std::exception_ptr> exceptions_;
  std::string message_;
};

/**
 * @brief 协程运行时环境
 * 提供在同步代码中运行异步协程的能力
 */
namespace Runtime {
template <typename ResultType>
static ResultType block_on(Task<ResultType>&& task) {
  LOG_TRACE("Runtime::block_on - blocking on task for result");
  std::optional<ResultType> result;
  std::exception_ptr exception_ptr;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<bool> is_completed = false;

  task.then([&](ResultType res) {
        LOG_TRACE("Runtime::block_on - task completed successfully");
        std::lock_guard lock(mtx);
        result = std::move(res);
        is_completed = true;
        cv.notify_one();
      })
      .catching([&](std::exception& e) {
        LOG_TRACE("Runtime::block_on - task completed with exception");
        std::lock_guard lock(mtx);
        exception_ptr = std::make_exception_ptr(e);
        is_completed = true;
        cv.notify_one();
      });
  task.start();
  LOG_TRACE("Runtime::block_on - waiting for task to complete");
  std::unique_lock lock(mtx);
  cv.wait(lock, [&is_completed]() { return is_completed.load(); });
  if (exception_ptr) {
    LOG_TRACE("Runtime::block_on - rethrowing exception from task");
    std::rethrow_exception(exception_ptr);
  }
  LOG_TRACE("Runtime::block_on - returning result from task");
  return *result;
}

static void block_on(Task<void>&& task) {
  LOG_TRACE("Runtime::block_on - blocking on void task for completion");
  // wrap a Task<int> around Task<void>
  auto wrapper_task = [&task]() -> Task<int> {
    co_await std::move(task);
    co_return 0;
  }();
  block_on(std::move(wrapper_task));
}

/**
 * @brief 启动多个协程并等待全部完成
 * @tparam Tasks 任务类型参数包
 * @param tasks 要执行的任务列表
 */
template <typename... Tasks>
static void join_all(Tasks&&... tasks) {
  // Attach completion callbacks first, then start all tasks concurrently,
  // and wait for all to finish. We collect the first exception (if any)
  // and rethrow it after all tasks complete.
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<size_t> remaining{sizeof...(Tasks)};
  std::vector<std::exception_ptr> exceptions;

  // Helper to attach callbacks for non-void and void Task types
  auto attach_wait = [&](auto& task) {
    using T = std::decay_t<decltype(task)>;

    // catching is common for all Task types
    task.catching([&](std::exception& e) {
      std::lock_guard lk(mtx);
      exceptions.push_back(std::make_exception_ptr(e));
      if (remaining.fetch_sub(1) == 1) cv.notify_one();
    });

    if constexpr (std::is_same_v<T, Task<void>>) {
      task.then([&]() {
        if (remaining.fetch_sub(1) == 1) cv.notify_one();
      });
    } else {
      task.then([&](auto) {
        if (remaining.fetch_sub(1) == 1) cv.notify_one();
      });
    }
  };

  // Attach callbacks to each task (evaluate left-to-right)
  (attach_wait(tasks), ...);

  // Start all tasks concurrently
  (tasks.start(), ...);

  // Wait until all tasks have completed
  std::unique_lock lk(mtx);
  cv.wait(lk, [&remaining]() { return remaining.load() == 0; });

  if (!exceptions.empty()) {
    // 抛出聚合异常，包含所有任务中的异常
    throw AggregateException(std::move(exceptions));
  }
}
};  // namespace Runtime

/**
 * @brief async_main 宏,简化协程主函数的编写
 * 用法: ASYNC_MAIN(MyAsyncScheduler) { co_return 0; }
 */
#define ASYNC_MAIN(SchedulerType)                                    \
  Task<int> async_main_impl();                                       \
  int main() {                                                       \
    try {                                                            \
      return koroutine::Runtime::block_on(async_main_impl());        \
    } catch (const std::exception& e) {                              \
      std::cerr << "Unhandled exception: " << e.what() << std::endl; \
      return 1;                                                      \
    }                                                                \
  }                                                                  \
  Task<int> async_main_impl()

/**
 * @brief 简化版 async_main,使用默认的调度器
 */
#define ASYNC_MAIN_DEFAULT()                                         \
  Task<int> async_main_impl();                                       \
  int main() {                                                       \
    try {                                                            \
      return koroutine::Runtime::block_on(async_main_impl());        \
    } catch (const std::exception& e) {                              \
      std::cerr << "Unhandled exception: " << e.what() << std::endl; \
      return 1;                                                      \
    }                                                                \
  }                                                                  \
  Task<int> async_main_impl()

}  // namespace koroutine
