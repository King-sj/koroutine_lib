#pragma once

#include "../coroutine_common.h"
#include "../executors/executor.h"

namespace koroutine {
struct DispatchAwaiter {
  explicit DispatchAwaiter(AbstractExecutor* executor) noexcept
      : _executor(executor) {}

  bool await_ready() const { return false; }

  void await_suspend(std::coroutine_handle<> handle) const {
    _executor->execute([handle]() { handle.resume(); });
  }

  void await_resume() {}

 private:
  AbstractExecutor* _executor;
};

}  // namespace koroutine