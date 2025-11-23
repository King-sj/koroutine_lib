#pragma once

#include <coroutine>
#include <memory>

#include "../executors/executor.h"

namespace koroutine {

class SwitchExecutorAwaiter {
 public:
  explicit SwitchExecutorAwaiter(std::shared_ptr<AbstractExecutor> executor)
      : executor_(std::move(executor)) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> handle) {
    executor_->execute([handle]() mutable { handle.resume(); });
  }

  void await_resume() const noexcept {}

 private:
  std::shared_ptr<AbstractExecutor> executor_;
};

inline SwitchExecutorAwaiter switch_to(
    std::shared_ptr<AbstractExecutor> executor) {
  return SwitchExecutorAwaiter(std::move(executor));
}

}  // namespace koroutine
