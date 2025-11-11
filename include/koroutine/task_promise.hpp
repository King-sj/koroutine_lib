#pragma once

#include <functional>
#include <list>
#include <mutex>

#include "awaiters/awaiter.hpp"
#include "awaiters/dispatch_awaiter.hpp"
#include "awaiters/sleep_awaiter.hpp"
#include "awaiters/switch_executor.hpp"
#include "awaiters/task_awaiter.hpp"
#include "coroutine_common.h"
#include "executor_manager.h"
#include "executors/executor.h"

namespace koroutine {

template <typename AwaiterImpl, typename R>
concept AwaiterImplRestriction =
    std::is_base_of<AwaiterBase<R>, AwaiterImpl>::value;

template <typename ResultType>
class Task;

// CRTP 基类 - Derived 是派生类 (TaskPromise<ResultType>)
template <typename ResultType, typename Derived>
struct TaskPromiseBase {
  auto initial_suspend() {
    LOG_TRACE("TaskPromise::initial_suspend - suspending initially");
    return std::suspend_always{};
  }

  auto final_suspend() noexcept { return std::suspend_always{}; }

  template <typename _ResultType>
  TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
    LOG_TRACE("TaskPromise::await_transform - transforming Task<_ResultType>");
    return TaskAwaiter<_ResultType>{std::move(task)};
  }

  template <typename _Rep, typename _Period>
  auto await_transform(std::chrono::duration<_Rep, _Period>&& duration) {
    LOG_TRACE("TaskPromise::await_transform - transforming sleep duration: ",
              duration.count());
    return await_transform(SleepAwaiter(duration.count()));
  }

  template <typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl,
                                    typename AwaiterImpl::ResultType>
  AwaiterImpl await_transform(AwaiterImpl&& awaiter) {
    LOG_TRACE("TaskPromise::await_transform - installing executor");
    awaiter.install_executor(executor.lock());
    return std::move(awaiter);
  }

  void unhandled_exception() {
    std::lock_guard lock(completion_lock);
    result = Result<ResultType>(std::current_exception());
    completion.notify_all();
    notify_callbacks();
  }

  void on_completed(std::function<void(Result<ResultType>)>&& func) {
    std::unique_lock lock(completion_lock);
    if (result.has_value()) {
      lock.unlock();
      func(*result);
    } else {
      completion_callbacks.push_back(std::move(func));
    }
  }

  void set_executor(std::shared_ptr<AbstractExecutor> ex) { executor = ex; }

  // CRTP: 通过派生类访问 get_return_object
  Task<ResultType> get_return_object() {
    return static_cast<Derived*>(this)->get_return_object_impl();
  }

 protected:
  std::optional<Result<ResultType>> result;
  std::mutex completion_lock;
  std::condition_variable completion;
  std::list<std::function<void(Result<ResultType>)>> completion_callbacks;
  std::weak_ptr<AbstractExecutor> executor =
      ExecutorManager::get_default_executor();

  void notify_callbacks() {
    for (auto& callback : completion_callbacks) {
      callback(*result);
    }
    completion_callbacks.clear();
  }
};

// 通用模板 - 非 void 类型
template <typename ResultType>
struct TaskPromise : TaskPromiseBase<ResultType, TaskPromise<ResultType>> {
  using Base = TaskPromiseBase<ResultType, TaskPromise<ResultType>>;
  using Base::completion;
  using Base::completion_lock;
  using Base::notify_callbacks;
  using Base::result;

  // CRTP 实现
  Task<ResultType> get_return_object_impl();

  void return_value(ResultType value) {
    LOG_TRACE("TaskPromise::return_value - returning value");
    std::lock_guard lock(completion_lock);
    result = Result<ResultType>(std::move(value));
    completion.notify_all();
    notify_callbacks();
  }

  ResultType get_result() {
    LOG_TRACE("TaskPromise::get_result - waiting for result");
    std::unique_lock lock(completion_lock);
    LOG_TRACE("TaskPromise::get_result - acquired lock");
    if (!result.has_value()) {
      LOG_TRACE("TaskPromise::get_result - waiting on condition variable");
      completion.wait(lock);
      LOG_TRACE("TaskPromise::get_result - woke up from wait");
    }
    LOG_TRACE("TaskPromise::get_result - returning result");
    return result->get_or_throw();
  }
};

// void 类型的特化
template <>
struct TaskPromise<void> : TaskPromiseBase<void, TaskPromise<void>> {
  using Base = TaskPromiseBase<void, TaskPromise<void>>;

  // CRTP 实现
  Task<void> get_return_object_impl();

  void return_void() {
    LOG_TRACE("TaskPromise<void>::return_void - returning void");
    std::lock_guard lock(completion_lock);
    result = Result<void>();
    completion.notify_all();
    notify_callbacks();
  }

  void get_result() {
    LOG_TRACE("TaskPromise<void>::get_result - waiting for result");
    std::unique_lock lock(completion_lock);
    LOG_TRACE("TaskPromise<void>::get_result - acquired lock");
    if (!result.has_value()) {
      LOG_TRACE(
          "TaskPromise<void>::get_result - waiting on condition variable");
      completion.wait(lock);
      LOG_TRACE("TaskPromise<void>::get_result - woke up from wait");
    }
    LOG_TRACE("TaskPromise<void>::get_result - returning result");
    result->get_or_throw();
  }
};

}  // namespace koroutine