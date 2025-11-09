#pragma once

#include <type_traits>

#include "../coroutine_common.h"
#include "../executors/executor.h"
#include "awaiter.hpp"

namespace koroutine {
template <typename ResultType>
class Task;

template <typename ResultType>
struct TaskAwaiter : public AwaiterBase<ResultType> {
  explicit TaskAwaiter(Task<ResultType>&& task) noexcept
      : task_(std::move(task)) {}

  TaskAwaiter(TaskAwaiter&&) noexcept = default;

  TaskAwaiter(TaskAwaiter&) = delete;

  TaskAwaiter& operator=(TaskAwaiter&) = delete;

 protected:
  void after_suspend() override {
    task_.finally([this]() { this->resume_unsafe(); });
  }

  void before_resume() override {
    if constexpr (std::is_void_v<ResultType>) {
      // Task::get_result() returns void for ResultType = void, just call it
      task_.get_result();
      this->_result = Result<void>();
    } else {
      this->_result = Result<ResultType>(task_.get_result());
    }
  }

 private:
  Task<ResultType> task_;
};
}  // namespace koroutine