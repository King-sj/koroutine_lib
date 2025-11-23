#pragma once

#include <functional>
#include <list>
#include <mutex>
#include <optional>

#include "awaiters/awaiter.hpp"
#include "awaiters/sleep_awaiter.hpp"
#include "awaiters/task_awaiter.hpp"
#include "cancellation.hpp"
#include "coroutine_common.h"
#include "scheduler_manager.h"

namespace koroutine {

template <typename AwaiterImpl, typename R>
concept AwaiterImplRestriction =
    std::is_base_of<AwaiterBase<R>, AwaiterImpl>::value;
template <typename ResultType, typename Derived>
class TaskBase;
template <typename ResultType>
class Task;

// CRTP 基类 - Derived 是派生类 (TaskPromise<ResultType>)
template <typename ResultType, typename Derived>
struct TaskPromiseBase {
  auto initial_suspend() {
    LOG_TRACE("TaskPromise::initial_suspend - suspending initially");
    return std::suspend_always{};
  }

  auto final_suspend() noexcept {
    // 在 ~task 中 调用 handle_.destroy()
    LOG_TRACE("TaskPromise::final_suspend - final suspend point");
    return std::suspend_always{};
  }

  template <typename _ResultType>
  TaskAwaiter<_ResultType> await_transform(Task<_ResultType>&& task) {
    LOG_TRACE("TaskPromise::await_transform - transforming Task<_ResultType>");
    auto awaiter = TaskAwaiter<_ResultType>{std::move(task)};
    awaiter.install_scheduler(scheduler.lock());
    return awaiter;
  }

  template <typename _Rep, typename _Period>
  auto await_transform(std::chrono::duration<_Rep, _Period>&& duration) {
    long long delay_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    LOG_TRACE("TaskPromise::await_transform - transforming sleep duration: ",
              delay_ms, " ms");
    auto awaiter = SleepAwaiter(delay_ms);
    awaiter.install_scheduler(scheduler.lock());
    return awaiter;
  }

  template <typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl,
                                    typename AwaiterImpl::ResultType>
  AwaiterImpl await_transform(AwaiterImpl&& awaiter) {
    LOG_TRACE(
        "TaskPromise::await_transform - installing scheduler and checking "
        "cancellation");

    // 检查取消状态
    if (cancel_token_ && cancel_token_->is_cancelled()) {
      LOG_WARN("TaskPromise::await_transform - operation cancelled");
      throw OperationCancelledException();
    }

    awaiter.install_scheduler(scheduler.lock());
    return std::move(awaiter);
  }

  // 通用的 await_transform，支持任意 awaitable（如 ScheduleAwaiter,
  // DispatchAwaiter 等）
  template <typename Awaitable>
  Awaitable&& await_transform(Awaitable&& awaitable) {
    LOG_TRACE("TaskPromise::await_transform - generic awaitable");

    // 检查取消状态
    if (cancel_token_ && cancel_token_->is_cancelled()) {
      LOG_WARN("TaskPromise::await_transform - operation cancelled");
      throw OperationCancelledException();
    }

    return std::forward<Awaitable>(awaitable);
  }

  void unhandled_exception() {
    std::lock_guard lock(completion_lock);
    result = Result<ResultType>(std::current_exception());
    completion.notify_all();

    // 恢复 continuation
    resume_continuation();
  }

  void set_scheduler(std::shared_ptr<AbstractScheduler> ex) { scheduler = ex; }

  bool is_started() const { return started; }
  void set_started() { started = true; }

  std::weak_ptr<AbstractScheduler> get_scheduler() { return scheduler; }

  /**
   * @brief 设置 continuation - 当前任务完成后要恢复的协程
   * @param handle 等待当前任务的协程句柄
   */
  void set_continuation(std::coroutine_handle<> handle) {
    LOG_TRACE("TaskPromise::set_continuation - setting continuation handle: ",
              handle.address());
    continuation_ = handle;
  }

  /**
   * @brief 设置取消令牌
   * @param token 取消令牌
   *
   * 当令牌被取消时，任务会在下一个 co_await 点检查并抛出
   * OperationCancelledException。
   */
  void set_cancellation_token(CancellationToken token) {
    LOG_TRACE(
        "TaskPromise::set_cancellation_token - setting cancellation token");
    cancel_token_ = token;

    // 注册取消回调
    // 注意：使用 weak reference 避免循环引用和生命周期问题
    auto weak_scheduler = scheduler;
    auto continuation_handle = &continuation_;
    auto result_ptr = &result;
    auto lock_ptr = &completion_lock;
    auto cv_ptr = &completion;

    token.on_cancel([weak_scheduler, continuation_handle, result_ptr, lock_ptr,
                     cv_ptr]() {
      LOG_INFO("TaskPromise - cancellation requested");
      // 设置异常结果
      {
        std::lock_guard lock(*lock_ptr);
        if (!result_ptr->has_value()) {
          *result_ptr = Result<ResultType>(
              std::make_exception_ptr(OperationCancelledException()));
          cv_ptr->notify_all();
        }
      }

      // 如果有 continuation，立即恢复（让它处理取消）
      if (*continuation_handle) {
        auto sched = weak_scheduler.lock();
        if (sched) {
          sched->schedule(
              ScheduleRequest(*continuation_handle,
                              ScheduleMetadata(ScheduleMetadata::Priority::High,
                                               "cancellation_continuation")),
              0);
        } else {
          LOG_ERROR(
              "TaskPromise - cancellation: no scheduler available to resume "
              "continuation");
          // 不再直接 resume，必须有调度器
        }
      }
    });
  }

  // CRTP: 通过派生类访问 get_return_object
  Task<ResultType> get_return_object() {
    return static_cast<Derived*>(this)->get_return_object_impl();
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
    if constexpr (std::is_void_v<ResultType>) {
      result->get_or_throw();
      return;
    } else {
      return result->get_or_throw();
    }
  }

 protected:
  friend class TaskBase<ResultType, Task<ResultType>>;

  std::optional<Result<ResultType>> result;
  std::mutex completion_lock;
  std::condition_variable completion;
  std::weak_ptr<AbstractScheduler> scheduler =
      SchedulerManager::get_default_scheduler();
  bool started = false;  // 防止重复启动

  // Continuation: 当前任务完成后要恢复的协程句柄
  std::coroutine_handle<> continuation_ = nullptr;

  // Cancellation token: 用于协作式取消
  std::optional<CancellationToken> cancel_token_;

 public:
  /**
   * @brief 恢复 continuation（通过调度器）
   */
  void resume_continuation() {
    if (continuation_) {
      LOG_TRACE("TaskPromise::resume_continuation - resuming continuation: ",
                continuation_.address());
      auto sched = scheduler.lock();
      if (sched) {
        // 通过调度器恢复 continuation
        ScheduleMetadata meta(
            ScheduleMetadata::Priority::Normal,
            "continuation_" + std::to_string((uintptr_t)this));
        sched->schedule(ScheduleRequest(continuation_, std::move(meta)), 0);
      } else {
        LOG_WARN(
            "TaskPromise::resume_continuation - no scheduler available, "
            "resuming directly");
        continuation_.resume();
      }
      continuation_ = nullptr;  // 避免重复恢复
    }
  }
};

// 通用模板 - 非 void 类型
template <typename ResultType>
struct TaskPromise : TaskPromiseBase<ResultType, TaskPromise<ResultType>> {
  using result_type = ResultType;
  using Base = TaskPromiseBase<ResultType, TaskPromise<ResultType>>;
  using Base::completion;
  using Base::completion_lock;
  using Base::result;

  // CRTP 实现
  Task<ResultType> get_return_object_impl();

  void return_value(ResultType value) {
    LOG_TRACE("TaskPromise::return_value - returning value");
    std::lock_guard lock(completion_lock);
    result = Result<ResultType>(std::move(value));
    completion.notify_all();

    // 恢复 continuation
    Base::resume_continuation();
  }
};

// void 类型的特化
template <>
struct TaskPromise<void> : TaskPromiseBase<void, TaskPromise<void>> {
  using result_type = void;
  using Base = TaskPromiseBase<void, TaskPromise<void>>;

  // CRTP 实现
  Task<void> get_return_object_impl();

  void return_void() {
    LOG_TRACE("TaskPromise<void>::return_void - returning void");
    std::lock_guard lock(completion_lock);
    result = Result<void>();
    completion.notify_all();

    // 恢复 continuation
    Base::resume_continuation();
  }
};

}  // namespace koroutine