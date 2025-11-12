#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <thread>

#include "coroutine_common.h"
#include "scheduler_manager.h"
#include "schedulers/scheduler.h"
#include "task.hpp"
namespace koroutine {

/**
 * @brief 协程运行时环境
 * 提供在同步代码中运行异步协程的能力
 */
class Runtime {
 public:
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
    // (tasks.get_result(), ...);
    (tasks.start(), ...);
    (block_on(std::move(tasks)), ...);
  }
};

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
