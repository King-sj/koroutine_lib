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
  ResultType get_result() { return handle_.promise().get_result(); }

  Task& then(std::function<void(ResultType)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        func(result.get_or_throw());
      } catch (std::exception& e) {
        // ignore.
      }
    });
    return *this;
  }
  Task& catching(std::function<void(std::exception&)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        result.get_or_throw();
      } catch (std::exception& e) {
        func(e);
      }
    });
    return *this;
  }
  Task& finally(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) { func(); });
    return *this;
  }
  Task& via(std::shared_ptr<AbstractExecutor> executor) {
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
        result.get_or_throw();
        func();
      } catch (std::exception& e) {
        // ignore.
      }
    });
    return *this;
  }
  Task& catching(std::function<void(std::exception&)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        result.get_or_throw();
      } catch (std::exception& e) {
        func(e);
      }
    });
    return *this;
  }
  Task& finally(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) { func(); });
    return *this;
  }
  Task& via(std::shared_ptr<AbstractExecutor> executor) {
    handle_.promise().set_executor(executor);
    return *this;
  }

 private:
  handle_type handle_;
};
}  // namespace koroutine

// get_return_object is defined in the implementation file to avoid
// instantiating Task before it is fully defined.