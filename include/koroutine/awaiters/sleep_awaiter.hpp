#pragma once

#include "../coroutine_common.h"
#include "../schedulers/scheduler.h"
#include "../schedulers/timer_scheduler.hpp"
#include "awaiter.hpp"

namespace koroutine {
struct SleepAwaiter : public AwaiterBase<void> {
  explicit SleepAwaiter(long long duration) noexcept : _duration(duration) {}

  template <typename _Rep, typename _Period>
  explicit SleepAwaiter(
      std::chrono::duration<_Rep, _Period>&& duration) noexcept
      : _duration(
            std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                .count()) {}

 protected:
  void after_suspend() override {
    if (_scheduler) {
      LOG_TRACE("SleepAwaiter::after_suspend - scheduling resume after ",
                _duration, " ms");
      // 使用 ScheduleRequest 调度恢复
      ScheduleMetadata meta(ScheduleMetadata::Priority::Normal,
                            "sleep_awaiter");
      _scheduler->schedule(ScheduleRequest(_caller_handle, std::move(meta)),
                           _duration);
    } else {
      LOG_ERROR(
          "SleepAwaiter::after_suspend - no scheduler bound, cannot sleep!");
      // 不再直接 resume，必须有调度器
      throw std::runtime_error(
          "SleepAwaiter::after_suspend - no scheduler bound");
    }
  }

  void before_resume() override { this->_result = Result<void>(); }

 private:
  long long _duration;
};

}  // namespace koroutine