#pragma once

#include <functional>
#include <list>
#include <mutex>

#include "awaiters/awaiter.hpp"
#include "awaiters/dispatch_awaiter.hpp"
#include "awaiters/sleep_awaiter.hpp"
#include "awaiters/task_awaiter.hpp"
#include "coroutine_common.h"
#include "executors/executor.h"

namespace koroutine {

template <typename AwaiterImpl, typename R>
concept AwaiterImplRestriction =
    std::is_base_of<AwaiterBase<R>, AwaiterImpl>::value;

template <typename ResultType, typename Executor>
class Task;

template <typename ResultType, typename Executor>
struct TaskPromise {
  DispatchAwaiter initial_suspend() { return DispatchAwaiter{&executor}; }
  auto final_suspend() noexcept { return std::suspend_always{}; }
  Task<ResultType, Executor> get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }

  template <typename _ResultType, typename _Executor>
  TaskAwaiter<_ResultType, _Executor> await_transform(
      Task<_ResultType, _Executor>&& task) {
    return TaskAwaiter<_ResultType, _Executor>{std::move(task)};
  }

  template <typename _Rep, typename _Period>
  TaskAwaiter<ResultType, Executor> await_transform(
      std::chrono::duration<_Rep, _Period>&& duration) {
    return await_transform(SleepAwaiter(duration));
  }

  template <typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl,
                                    typename AwaiterImpl::ResultType>
  AwaiterImpl await_transform(AwaiterImpl awaiter) {
    awaiter.install_executor(&executor);
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

 private:
  std::optional<Result<ResultType>> result;

  std::mutex completion_lock;
  std::condition_variable completion;

  std::list<std::function<void(Result<ResultType>)>> completion_callbacks;

  Executor executor;

  void notify_callbacks() {
    for (auto& callback : completion_callbacks) {
      callback(*result);
    }
    completion_callbacks.clear();
  }
};

template <typename Executor>
struct TaskPromise<void, Executor> {
  DispatchAwaiter initial_suspend() { return DispatchAwaiter{&executor}; }
  auto final_suspend() noexcept { return std::suspend_always{}; }
  Task<void, Executor> get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }

  template <typename _ResultType, typename _Executor>
  TaskAwaiter<_ResultType, _Executor> await_transform(
      Task<_ResultType, _Executor>&& task) {
    return await_transform(
        TaskAwaiter<_ResultType, _Executor>(std::move(task)));
  }

  template <typename _Rep, typename _Period>
  SleepAwaiter await_transform(
      std::chrono::duration<_Rep, _Period>&& duration) {
    return await_transform(SleepAwaiter(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration)
            .count()));
  }

  template <typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl,
                                    typename AwaiterImpl::ResultType>
  AwaiterImpl await_transform(AwaiterImpl&& awaiter) {
    awaiter.install_executor(&executor);
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

 private:
  std::optional<Result<void>> result;

  std::mutex completion_lock;
  std::condition_variable completion;

  std::list<std::function<void(Result<void>)>> completion_callbacks;

  Executor executor;

  void notify_callbacks() {
    for (auto& callback : completion_callbacks) {
      callback(*result);
    }
    completion_callbacks.clear();
  }
};

}  // namespace koroutine