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

  explicit TaskBase(handle_type handle) : handle_(handle) {
    LOG_INFO("TaskBase::constructor - handle address: ", handle_.address());
  }

  ~TaskBase() {
    LOG_INFO("TaskBase::~TaskBase - destroying, handle address: ",
             handle_ ? handle_.address() : nullptr, " this=", (void*)this);
    if (handle_ && handle_.address() != nullptr) {
      LOG_INFO("TaskBase::~TaskBase - destroying coroutine handle");
      handle_.destroy();
    } else if (handle_) {
      LOG_ERROR(
          "TaskBase::~TaskBase - handle is not null but address is null!");
    }
  }

  TaskBase(const TaskBase&) = delete;
  TaskBase& operator=(const TaskBase&) = delete;

  TaskBase(TaskBase&& task) noexcept
      : handle_(std::exchange(task.handle_, nullptr)) {
    LOG_INFO("TaskBase::move constructor - moved TaskBase<ResultType>, from: ",
             handle_ ? handle_.address() : nullptr,
             " source now: ", task.handle_ ? task.handle_.address() : nullptr,
             " this=", (void*)this, " source=", (void*)&task);
  }

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
    handle_.promise().on_completed([func](Result<ResultType>& result) {
      // 不调用 get_or_throw()，只检查是否有异常
      if (result.has_exception()) {
        try {
          result.rethrow_exception();
        } catch (std::exception& e) {
          LOG_TRACE("Task::catching - exception caught, invoking callback");
          func(e);
        }
      }
    });
    return static_cast<Derived&>(*this);
  }

  Derived& finally(std::function<void()>&& func) {
    handle_.promise().on_completed([func](Result<ResultType>& _) {
      LOG_TRACE("Task::finally - invoking finally callback");
      func();
    });
    return static_cast<Derived&>(*this);
  }

  void start() {
    // 防止重复启动
    if (handle_.promise().is_started()) {
      LOG_ERROR("Task::start - task already started, ignoring duplicate start");
      return;
    }
    handle_.promise().set_started();

    std::shared_ptr<AbstractScheduler> scheduler =
        handle_.promise().get_scheduler().lock();
    if (!scheduler) {
      LOG_WARN("Task::start - no scheduler set, using default scheduler");
      scheduler = SchedulerManager::get_default_scheduler();
      handle_.promise().set_scheduler(scheduler);
    }
    LOG_TRACE("Task::start - starting task with scheduler, handle: ",
              handle_.address());
    scheduler->schedule(
        [h = handle_]() {
          LOG_TRACE("Task::start - resuming coroutine, handle: ", h.address());
          h.resume();
          LOG_TRACE("Task::start - coroutine resumed, done: ", h.done());
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
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&& task) noexcept : Base(std::move(task)) {
    LOG_INFO("Task::move constructor - moved Task<ResultType>");
  }
  Task& operator=(Task&&) noexcept = default;
  ~Task() = default;

  Task& then(std::function<void(ResultType)>&& func) {
    handle_.promise().on_completed([func](Result<ResultType>& result) {
      if (!result.has_exception()) {
        try {
          LOG_TRACE("Task::then - invoking then callback");
          // 注意：这里调用 get_or_throw() 会移动值
          // 如果有多个 then 回调，只有第一个能拿到值
          func(result.get_or_throw());
        } catch (std::exception& e) {
          LOG_WARN("Task::then - exception in then callback: ", e.what());
        }
      }
    });
    return *this;
  }

 protected:
  friend class TaskAwaiter<ResultType>;
  // blocking for result or throw on exception
  ResultType get_result() {
    LOG_TRACE("Task::get_result - getting result from promise");
    return handle_.promise().get_result();
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
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&& task) noexcept : Base(std::move(task)) {
    LOG_INFO("Task::move constructor - moved Task<void>");
  }
  Task& operator=(Task&&) noexcept = default;

  ~Task() = default;

  Task& then(std::function<void()>&& func) {
    handle_.promise().on_completed([func](Result<void>& result) {
      if (!result.has_exception()) {
        try {
          LOG_TRACE("Task<void>::then - invoking then callback");
          result.get_or_throw();
          func();
        } catch (std::exception& e) {
          LOG_WARN("Task<void>::then - exception in then callback: ", e.what());
        }
      }
    });
    return *this;
  }

 protected:
  friend class TaskAwaiter<void>;
  void get_result() { handle_.promise().get_result(); }
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