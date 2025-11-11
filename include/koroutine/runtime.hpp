#pragma once

#include <future>
#include <memory>
#include <thread>

#include "coroutine_common.h"
#include "executors/async_executor.h"
#include "executors/looper_executor.h"
#include "task.hpp"

namespace koroutine {

/**
 * @brief 协程运行时环境
 * 提供在同步代码中运行异步协程的能力
 */
class Runtime {
 public:
  /**
   * @brief 在默认执行器上运行一个 Task 并阻塞等待结果
   * @tparam ResultType 返回值类型
   * @tparam Executor 执行器类型
   * @param task 要执行的任务
   * @return 任务的执行结果
   */
  template <typename ResultType>
  static ResultType block_on(Task<ResultType>&& task) {
    LOG_TRACE("Runtime::block_on - blocking on task for result");
    return task.get_result();
  }

  /**
   * @brief 在默认执行器上运行一个 Task(void) 并阻塞等待完成
   * @tparam Executor 执行器类型
   * @param task 要执行的任务
   */
  static void block_on(Task<void>&& task) { task.get_result(); }

  /**
   * @brief 使用 LooperExecutor 运行协程主函数
   * 适合需要事件循环的场景
   * @tparam Func 协程函数类型
   * @param func 协程函数
   * @return 协程执行结果
   */
  template <typename Func>
    requires std::is_invocable_v<Func>
  static auto run_with_looper(Func&& func) -> decltype(func().get_result()) {
    auto task = func();
    return task.get_result();
  }

  /**
   * @brief 使用 AsyncExecutor 运行协程主函数
   * 适合并发执行的场景
   * @tparam Func 协程函数类型
   * @param func 协程函数
   * @return 协程执行结果
   */
  template <typename Func>
    requires std::is_invocable_v<Func>
  static auto run_async(Func&& func) -> decltype(func().get_result()) {
    auto task = func();
    return task.get_result();
  }

  /**
   * @brief 启动多个协程并等待全部完成
   * @tparam Tasks 任务类型参数包
   * @param tasks 要执行的任务列表
   */
  template <typename... Tasks>
  static void join_all(Tasks&&... tasks) {
    (tasks.get_result(), ...);
  }
};

/**
 * @brief async_main 宏,简化协程主函数的编写
 * 用法: ASYNC_MAIN(MyAsyncExecutor) { co_return 0; }
 */
#define ASYNC_MAIN(ExecutorType)                                     \
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
 * @brief 简化版 async_main,使用默认的 AsyncExecutor
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
