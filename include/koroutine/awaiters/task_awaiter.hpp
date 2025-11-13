#pragma once

#include <type_traits>

#include "../coroutine_common.h"
#include "awaiter.hpp"

namespace koroutine {
template <typename ResultType>
class Task;

template <typename ResultType>
struct TaskAwaiter : public AwaiterBase<ResultType> {
  explicit TaskAwaiter(Task<ResultType>&& task) noexcept
      : task_(std::move(task)) {}

  TaskAwaiter(TaskAwaiter&& other) noexcept
      : AwaiterBase<ResultType>(std::move(other)),
        task_(std::move(other.task_)) {
    LOG_INFO("TaskAwaiter::move constructor - moved TaskAwaiter");
  }

  TaskAwaiter(TaskAwaiter&) = delete;

  TaskAwaiter& operator=(TaskAwaiter&) = delete;
  virtual bool await_ready() const override {
    LOG_TRACE("TaskAwaiter::await_ready - checking if task is ready : ",
              task_.handle_.done());
    // A task is ready if its coroutine handle is done
    return task_.handle_.done();
  }

 protected:
  void after_suspend() override {
    LOG_TRACE("TaskAwaiter::after_suspend - suspending on task");
    task_.finally([this]() {
      LOG_TRACE("TaskAwaiter::after_suspend - task completed");
      this->resume_unsafe();
      LOG_TRACE("TaskAwaiter::after_suspend - resumed awaiting coroutine");
    });
    task_.start();
  }

  void before_resume() override {
    LOG_TRACE("TaskAwaiter::before_resume - retrieving task result");
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