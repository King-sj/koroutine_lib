#pragma once

#include "coroutine_common.h"
#include "scheduler_manager.h"
#include "task_promise.hpp"

namespace koroutine {
class AbstractScheduler;

// CRTP 基类 - 包含公共的任务管理逻辑
template <typename ResultType, typename Derived>
class TaskBase {
 public:
  using promise_type = TaskPromise<ResultType>;
  using handle_type = std::coroutine_handle<promise_type>;

  explicit TaskBase(handle_type handle) : handle_(handle) {}

  ~TaskBase() {
    if (handle_) {
      handle_.destroy();
    }
  }

  TaskBase(const TaskBase&) = delete;
  TaskBase& operator=(const TaskBase&) = delete;

  TaskBase(TaskBase&& task) noexcept
      : handle_(std::exchange(task.handle_, {})) {}

  TaskBase& operator=(TaskBase&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  Derived& catching(std::function<void(std::exception&)>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        LOG_TRACE("Task::catching - checking for exception");
        result.get_or_throw();
      } catch (std::exception& e) {
        LOG_TRACE("Task::catching - exception caught, invoking callback");
        func(e);
      }
    });
    return static_cast<Derived&>(*this);
  }

  Derived& finally(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) {
      LOG_TRACE("Task::finally - invoking finally callback");
      func();
    });
    return static_cast<Derived&>(*this);
  }

  void start() {
    std::shared_ptr<AbstractScheduler> scheduler =
        handle_.promise().get_scheduler().lock();
    if (!scheduler) {
      LOG_WARN("Task::start - no scheduler set, using default scheduler");
      scheduler = SchedulerManager::get_default_scheduler();
    }
    LOG_TRACE("Task::start - starting task with scheduler");
    handle_.promise().set_scheduler(scheduler);
    scheduler->schedule(
        [h = handle_]() {
          LOG_TRACE("Task::start - resuming coroutine");
          h.resume();
          LOG_TRACE("Task::start - coroutine resumed");
        },
        0);
  }

 protected:
  handle_type handle_;
};

// 通用模板 - 非 void 类型
template <typename ResultType>
class Task : public TaskBase<ResultType, Task<ResultType>> {
 public:
  using Base = TaskBase<ResultType, Task<ResultType>>;
  using promise_type = typename Base::promise_type;
  using handle_type = typename Base::handle_type;
  using Base::handle_;

  explicit Task(handle_type handle) : Base(handle) {}

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
};

// void 类型的特化
template <>
class Task<void> : public TaskBase<void, Task<void>> {
 public:
  using Base = TaskBase<void, Task<void>>;
  using promise_type = typename Base::promise_type;
  using handle_type = typename Base::handle_type;
  using Base::handle_;

  explicit Task(handle_type handle) : Base(handle) {}

  void get_result() { handle_.promise().get_result(); }

  Task& then(std::function<void()>&& func) {
    handle_.promise().on_completed([func](auto result) {
      try {
        LOG_TRACE("Task<void>::then - invoking then callback");
        result.get_or_throw();
        func();
      } catch (std::exception& e) {
        LOG_WARN("Task<void>::then - exception in then callback: ", e.what());
        // ignore.
      }
    });
    return *this;
  }
};

// get_return_object_impl 的实现必须在 Task 完全定义之后
// 因为它返回 Task 对象（CRTP 模式）
template <typename ResultType>
inline Task<ResultType> TaskPromise<ResultType>::get_return_object_impl() {
  LOG_TRACE("TaskPromise::get_return_object_impl - creating Task<ResultType>");
  return Task<ResultType>{
      std::coroutine_handle<TaskPromise<ResultType>>::from_promise(*this)};
}

inline Task<void> TaskPromise<void>::get_return_object_impl() {
  LOG_TRACE("TaskPromise<void>::get_return_object_impl - creating Task<void>");
  return Task<void>{
      std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}

}  // namespace koroutine