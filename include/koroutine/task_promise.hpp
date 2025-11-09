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

template <typename ResultType>
struct TaskPromise {
  auto initial_suspend() { return DispatchAwaiter{executor.lock()}; }
  auto final_suspend() noexcept { return std::suspend_always{}; }
  Task<ResultType> get_return_object() {
    return Task<ResultType>{
        std::coroutine_handle<TaskPromise<ResultType>>::from_promise(*this)};
  }

  template <typename _ResultType>
  TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
    return TaskAwaiter<_ResultType>{std::move(task)};
  }

  template <typename _Rep, typename _Period>
  auto await_transform(std::chrono::duration<_Rep, _Period>&& duration) {
    return await_transform(SleepAwaiter(duration.count()));
  }

  template <typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl,
                                    typename AwaiterImpl::ResultType>
  AwaiterImpl await_transform(AwaiterImpl awaiter) {
    LOG_TRACE("TaskPromise::await_transform - installing executor");
    awaiter.install_executor(executor.lock());
    return awaiter;
  }

  void unhandled_exception() {
    std::lock_guard lock(completion_lock);
    result = Result<ResultType>(std::current_exception());
    completion.notify_all();
    notify_callbacks();
  }

  void return_value(ResultType value) {
    std::lock_guard lock(completion_lock);
    result = Result<ResultType>(std::move(value));
    completion.notify_all();
    notify_callbacks();
  }

  // blocking for result or throw on exception
  ResultType get_result() {
    std::unique_lock lock(completion_lock);
    if (!result.has_value()) {
      completion.wait(lock);
    }
    return result->get_or_throw();
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

 private:
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

template <>
struct TaskPromise<void> {
  DispatchAwaiter initial_suspend() { return DispatchAwaiter{executor.lock()}; }
  auto final_suspend() noexcept { return std::suspend_always{}; }
  Task<void> get_return_object();

  TaskAwaiter<void> await_transform(Task<void>&& task);

  template <typename _ResultType>
  TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
    return TaskAwaiter<_ResultType>{std::move(task)};
  }

  template <typename _Rep, typename _Period>
  auto await_transform(std::chrono::duration<_Rep, _Period>&& duration) {
    return await_transform(SleepAwaiter(duration));
  }

  template <typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl,
                                    typename AwaiterImpl::ResultType>
  AwaiterImpl await_transform(AwaiterImpl&& awaiter) {
    // automatically transfer executor
    awaiter.install_executor(executor.lock());
    return awaiter;
  }

  void get_result() {
    // blocking for result or throw on exception
    std::unique_lock lock(completion_lock);
    if (!result.has_value()) {
      completion.wait(lock);
    }
    result->get_or_throw();
  }

  void unhandled_exception() {
    std::lock_guard lock(completion_lock);
    result = Result<void>(std::current_exception());
    completion.notify_all();
    notify_callbacks();
  }

  void return_void() {
    std::lock_guard lock(completion_lock);
    result = Result<void>();
    completion.notify_all();
    notify_callbacks();
  }

  void on_completed(std::function<void(Result<void>)>&& func) {
    std::unique_lock lock(completion_lock);
    if (result.has_value()) {
      lock.unlock();
      func(*result);
    } else {
      completion_callbacks.push_back(std::move(func));
    }
  }

  void set_executor(std::shared_ptr<AbstractExecutor> ex) { executor = ex; }

 private:
  std::optional<Result<void>> result;

  std::mutex completion_lock;
  std::condition_variable completion;

  std::list<std::function<void(Result<void>)>> completion_callbacks;

  std::weak_ptr<AbstractExecutor> executor =
      ExecutorManager::get_default_executor();

  void notify_callbacks() {
    for (auto& callback : completion_callbacks) {
      callback(*result);
    }
    completion_callbacks.clear();
  }
};

}  // namespace koroutine