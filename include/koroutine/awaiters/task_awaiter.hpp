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
      : task_(std::move(task)) {
    LOG_INFO("TaskAwaiter::constructor - this=", (void*)this);
  }

  TaskAwaiter(TaskAwaiter&& other) noexcept
      : AwaiterBase<ResultType>(std::move(other)),
        task_(std::move(other.task_)) {
    LOG_INFO("TaskAwaiter::move constructor - moved TaskAwaiter, this=",
             (void*)this, " source=", (void*)&other);
  }

  ~TaskAwaiter() {
    LOG_INFO("TaskAwaiter::~TaskAwaiter - destroying, this=", (void*)this);
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
    LOG_TRACE(
        "TaskAwaiter::after_suspend - setting continuation and starting task");
    // 使用新的 continuation 机制
    // 当 task 完成时，会通过调度器自动恢复我们的协程
    task_.handle_.promise().set_continuation(this->_caller_handle);
    task_.start();
  }

  void before_resume() override {
    LOG_TRACE("TaskAwaiter::before_resume - retrieving result from task");
    // 直接调用 task_.get_result()，它会从 Promise 获取并移动 Result
    if constexpr (std::is_void_v<ResultType>) {
      task_.get_result();
      this->_result = Result<void>();
    } else {
      this->_result = Result<ResultType>(std::move(task_.get_result()));
    }
  }

 private:
  Task<ResultType> task_;
};
}  // namespace koroutine