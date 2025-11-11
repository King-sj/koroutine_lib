#pragma once

#include "coroutine_common.h"
#include "executors/noop_executor.h"
#include "task_promise.hpp"

namespace koroutine {
class AbstractExecutor;
template <typename ResultType>
class Task {
 public:
  using promise_type = TaskPromise<ResultType>;
  using handle_type = std::coroutine_handle<promise_type>;

  explicit Task(handle_type handle) : handle_(handle) {}
  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  Task(Task&& task) noexcept : handle_(std::exchange(task.handle_, {})) {}

  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  // blocking for result or throw on exception
  ResultType get_result() {
    LOG_TRACE("Task::get_result - getting result from promise");
    return handle_.promise().get_result();
  }

  Task& then(std::function<void(ResultType)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        LOG_TRACE("Task::then - invoking then callback");
        func(result.get_or_throw());
      } catch (std::exception& e) {
        LOG_WARN("Task::then - exception in then callback: ", e.what());
        // ignore.
      }
    });
    return *this;
  }
  Task& catching(std::function<void(std::exception&)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        LOG_TRACE("Task::catching - checking for exception");
        result.get_or_throw();
      } catch (std::exception& e) {
        LOG_TRACE("Task::catching - exception caught, invoking callback");
        func(e);
      }
    });
    return *this;
  }
  Task& finally(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) {
      LOG_TRACE("Task::finally - invoking finally callback");
      func();
    });
    return *this;
  }
  Task& via(std::shared_ptr<AbstractExecutor> executor) {
    LOG_TRACE("Task::via - setting executor for task");
    handle_.promise().set_executor(executor);
    return *this;
  }

 private:
  handle_type handle_;
};

template <>
struct Task<void> {
  using promise_type = TaskPromise<void>;
  using handle_type = std::coroutine_handle<promise_type>;
  explicit Task(handle_type handle) : handle_(handle) {}
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&& task) noexcept : handle_(std::exchange(task.handle_, {})) {}
  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }
  void get_result() { handle_.promise().get_result(); }

  Task& then(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        LOG_TRACE("Task<void>::then - invoking then callback");
        result.get_or_throw();
        func();
      } catch (std::exception& e) {
        // ignore.
        LOG_WARN("Task<void>::then - exception in then callback: ", e.what());
      }
    });
    return *this;
  }
  Task& catching(std::function<void(std::exception&)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        LOG_TRACE("Task<void>::catching - checking for exception");
        result.get_or_throw();
      } catch (std::exception& e) {
        LOG_TRACE("Task<void>::catching - exception caught, invoking callback");
        func(e);
      }
    });
    return *this;
  }
  Task& finally(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) {
      LOG_TRACE("Task<void>::finally - invoking finally callback");
      func();
    });
    return *this;
  }
  Task& via(std::shared_ptr<AbstractExecutor> executor) {
    LOG_TRACE("Task<void>::via - setting executor for task");
    handle_.promise().set_executor(executor);
    return *this;
  }

 private:
  handle_type handle_;
};
}  // namespace koroutine

// get_return_object is defined in the implementation file to avoid
// instantiating Task before it is fully defined.