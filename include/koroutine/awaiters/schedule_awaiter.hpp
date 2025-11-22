#pragma once

#include <chrono>
#include <coroutine>
#include <memory>

#include "../debug.h"
#include "../schedulers/schedule_request.hpp"

namespace koroutine {

// 前向声明
class AbstractScheduler;

/**
 * @brief 调度延迟 awaiter
 *
 * 用于实现 co_await scheduler->schedule(delay_ms) 语法。
 * 允许协程在指定的调度器上延迟恢复。
 */
class ScheduleAwaiter {
 public:
  /**
   * @brief 构造调度awaiter
   * @param scheduler 目标调度器
   * @param delay_ms 延迟时间（毫秒）
   */
  ScheduleAwaiter(std::shared_ptr<AbstractScheduler> scheduler,
                  long long delay_ms)
      : scheduler_(std::move(scheduler)), delay_ms_(delay_ms) {
    LOG_TRACE("ScheduleAwaiter::constructor - delay_ms: ", delay_ms_);
  }

  /**
   * @brief 检查是否可以立即恢复
   * @return 如果延迟为0则返回true
   */
  bool await_ready() const noexcept {
    bool ready = (delay_ms_ == 0);
    LOG_TRACE("ScheduleAwaiter::await_ready - ready: ", ready);
    return ready;
  }

  /**
   * @brief 挂起协程并调度恢复
   * @param handle 当前协程句柄
   */
  void await_suspend(std::coroutine_handle<> handle) {
    LOG_TRACE("ScheduleAwaiter::await_suspend - scheduling with delay: ",
              delay_ms_);
    if (scheduler_) {
      ScheduleMetadata meta(ScheduleMetadata::Priority::Normal,
                            "schedule_awaiter");
      scheduler_->schedule(ScheduleRequest(handle, std::move(meta)), delay_ms_);
    } else {
      LOG_ERROR("ScheduleAwaiter::await_suspend - null scheduler!");
      // Fallback: 立即恢复
      handle.resume();
    }
  }

  /**
   * @brief 恢复时调用（无返回值）
   */
  void await_resume() noexcept {
    LOG_TRACE("ScheduleAwaiter::await_resume - resumed after delay");
  }

 private:
  std::shared_ptr<AbstractScheduler> scheduler_;
  long long delay_ms_;
};

/**
 * @brief 调度器切换 awaiter
 *
 * 用于实现 co_await scheduler->dispatch_to() 语法。
 * 允许协程切换到指定的调度器执行。
 */
class DispatchAwaiter {
 public:
  /**
   * @brief 构造调度器切换awaiter
   * @param scheduler 目标调度器
   */
  explicit DispatchAwaiter(std::shared_ptr<AbstractScheduler> scheduler)
      : scheduler_(std::move(scheduler)) {
    LOG_TRACE("DispatchAwaiter::constructor");
  }

  /**
   * @brief 总是需要挂起以切换调度器
   */
  bool await_ready() const noexcept {
    LOG_TRACE("DispatchAwaiter::await_ready - always false");
    return false;
  }

  /**
   * @brief 挂起协程并在目标调度器上恢复
   * @param handle 当前协程句柄
   */
  void await_suspend(std::coroutine_handle<> handle) {
    LOG_TRACE("DispatchAwaiter::await_suspend - switching scheduler");
    if (scheduler_) {
      ScheduleMetadata meta(ScheduleMetadata::Priority::Normal,
                            "dispatch_awaiter");
      scheduler_->schedule(ScheduleRequest(handle, std::move(meta)), 0);
    } else {
      LOG_ERROR(
          "DispatchAwaiter::await_suspend - null scheduler, cannot dispatch!");
      // 不再 fallback 直接 resume，必须有调度器
    }
  }

  /**
   * @brief 恢复时调用（无返回值）
   */
  void await_resume() noexcept {
    LOG_TRACE("DispatchAwaiter::await_resume - resumed on new scheduler");
  }

 private:
  std::shared_ptr<AbstractScheduler> scheduler_;
};

}  // namespace koroutine
