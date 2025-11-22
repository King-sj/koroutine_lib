#pragma once
#include <coroutine>
#include <functional>
#include <memory>

#include "koroutine/debug.h"
#include "schedule_request.hpp"

namespace koroutine {

// 前向声明
class ScheduleAwaiter;
class DispatchAwaiter;

/**
 * @brief 抽象调度器接口
 *
 * 提供协程调度的核心功能。调度器负责决定何时何地执行协程。
 */
class AbstractScheduler
    : public std::enable_shared_from_this<AbstractScheduler> {
 public:
  virtual ~AbstractScheduler() = default;

  /**
   * @brief 调度协程句柄（核心接口）
   * @param request 调度请求，包含协程句柄和元数据
   * @param delay_ms 延迟时间（毫秒），0表示立即执行
   */
  virtual void schedule(ScheduleRequest request, long long delay_ms = 0) = 0;

  /**
   * @brief 返回一个awaitable，用于延迟执行
   * @param delay_ms 延迟时间（毫秒）
   * @return ScheduleAwaiter 可以被 co_await 的对象
   *
   * 使用示例：
   * @code
   * co_await scheduler->schedule(100);  // 延迟100ms
   * @endcode
   */
  ScheduleAwaiter schedule(long long delay_ms);

  /**
   * @brief 返回一个awaitable，用于切换到此调度器
   * @return DispatchAwaiter 可以被 co_await 的对象
   *
   * 使用示例：
   * @code
   * co_await scheduler->dispatch_to();  // 切换到这个调度器执行
   * @endcode
   */
  DispatchAwaiter dispatch_to();
};

}  // namespace koroutine

// 包含实现文件（因为需要完整的 awaiter 定义）
#include "koroutine/awaiters/schedule_awaiter.hpp"

namespace koroutine {

// 实现成员函数
inline ScheduleAwaiter AbstractScheduler::schedule(long long delay_ms) {
  return ScheduleAwaiter(shared_from_this(), delay_ms);
}

inline DispatchAwaiter AbstractScheduler::dispatch_to() {
  return DispatchAwaiter(shared_from_this());
}

}  // namespace koroutine
