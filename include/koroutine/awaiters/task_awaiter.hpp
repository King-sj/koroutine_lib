#pragma once

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

  TaskAwaiter(TaskAwaiter&& awaiter) noexcept
      : AwaiterBase<ResultType>(std::move(awaiter)),
        task_(std::move(awaiter.task_)) {}

  TaskAwaiter(TaskAwaiter&) = delete;

  TaskAwaiter& operator=(TaskAwaiter&) = delete;

 protected:
  void after_suspend() override {
    task_.finally([this]() { this->resume_unsafe(); });
  }

  void before_resume() override {
    this->_result = Result<ResultType>(task_.get_result());
  }

 private:
  Task<ResultType> task_;
};
}  // namespace koroutine