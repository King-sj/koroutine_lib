#pragma once

#include "../coroutine_common.h"
#include "../executors/executor.h"
#include "../schedulers/scheduler.hpp"
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
    static Scheduler scheduler;
    scheduler.schedule([this] { this->resume(); }, _duration);
  }

 private:
  long long _duration;
};

}  // namespace koroutine