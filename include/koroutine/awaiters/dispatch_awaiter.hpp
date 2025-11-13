#pragma once

#include "../coroutine_common.h"
#include "../executors/executor.h"

namespace koroutine {
struct DispatchAwaiter {
  explicit DispatchAwaiter(std::shared_ptr<AbstractExecutor> executor) noexcept
      : _executor(executor) {}

  bool await_ready() const { return false; }

  void await_suspend(std::coroutine_handle<> handle) const {
    if (_executor) {
      LOG_TRACE("DispatchAwaiter: resuming coroutine on specified executor.");
      _executor->execute([handle]() { handle.resume(); });
    } else {
      // no executor bound: resume immediately on current thread
      LOG_WARN(
          "DispatchAwaiter: no executor bound, resuming on current thread.");
      handle.resume();
    }
  }

  void await_resume() { LOG_TRACE("DispatchAwaiter: await_resume called."); }

 private:
  std::shared_ptr<AbstractExecutor> _executor;
};

}  // namespace koroutine