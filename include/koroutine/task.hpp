#pragma once

#include "coroutine_common.h"
#include "scheduler_manager.h"
#include "task_promise.hpp"

namespace koroutine {
class AbstractScheduler;

// 前置声明
template <typename ResultType>
class Task;

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

  /**
   * @brief 捕获异常并处理，返回新的 Task
   * @param func 异常处理函数
   * @return 新的 Task，可以继续链式调用
   *
   * 用法：
   * @code
   * co_await task()
   *   .catching([](std::exception& e) {
   *     std::cout << "Error: " << e.what() << std::endl;
   *   });
   * @endcode
   */
  Task<ResultType> catching(std::function<void(std::exception&)>&& func) && {
    auto prev_handle = std::exchange(handle_, nullptr);
    return [](handle_type prev_handle, auto func) -> Task<ResultType> {
      try {
        if constexpr (std::is_void_v<ResultType>) {
          co_await Task<ResultType>(prev_handle);
        } else {
          co_return co_await Task<ResultType>(prev_handle);
        }
      } catch (std::exception& e) {
        LOG_TRACE("Task::catching - exception caught, invoking callback");
        func(e);
        throw;  // 重新抛出异常
      }
    }(prev_handle, std::move(func));
  }

  /**
   * @brief 最终清理操作，无论成功还是失败都会执行，返回新的 Task
   * @param func 清理函数
   * @return 新的 Task，可以继续链式调用
   *
   * 用法：
   * @code
   * co_await task()
   *   .finally([]() {
   *     std::cout << "Cleanup" << std::endl;
   *   });
   * @endcode
   */
  Task<ResultType> finally(std::function<void()>&& func) && {
    auto prev_handle = std::exchange(handle_, nullptr);
    return [](handle_type prev_handle, auto func) -> Task<ResultType> {
      try {
        if constexpr (std::is_void_v<ResultType>) {
          co_await Task<ResultType>(prev_handle);
          LOG_TRACE("Task::finally - invoking finally callback (success)");
          func();
        } else {
          auto result = co_await Task<ResultType>(prev_handle);
          LOG_TRACE("Task::finally - invoking finally callback (success)");
          func();
          co_return result;
        }
      } catch (...) {
        LOG_TRACE("Task::finally - invoking finally callback (exception)");
        func();
        throw;  // 重新抛出异常
      }
    }(prev_handle, std::move(func));
  }

  /**
   * @brief 设置取消令牌
   * @param token 取消令牌
   * @return 返回任务自身以支持链式调用
   *
   * 使用示例:
   * @code
   * CancellationToken token;
   * auto task = my_async_operation()
   *   .with_cancellation(token)
   *   .start();
   *
   * // 稍后取消
   * token.cancel();
   * @endcode
   */
  Derived& with_cancellation(CancellationToken token) {
    LOG_TRACE("Task::with_cancellation - setting cancellation token");
    handle_.promise().set_cancellation_token(token);
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

    // 使用 ScheduleRequest 调度协程
    ScheduleMetadata meta(ScheduleMetadata::Priority::Normal, "task_start");
    scheduler->schedule(ScheduleRequest(handle_, std::move(meta)), 0);
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

  /**
   * @brief 任务成功完成时执行回调，返回新的 Task
   * @param func 处理结果的回调函数
   * @return 新的 Task，可以继续链式调用
   *
   * 支持两种用法：
   * 1. 消费结果，返回 void:
   *    task().then([](ResultType result) { ... })  -> Task<void>
   * 2. 转换结果，返回新值:
   *    task().then([](ResultType result) -> NewType { return ...; })  ->
   * Task<NewType>
   */
  template <typename Func>
  auto then(Func&& func) &&;

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

  /**
   * @brief 任务成功完成时执行回调，返回新的 Task<void>
   * @param func 回调函数
   * @return 新的 Task<void>，可以继续链式调用
   *
   * 支持两种用法：
   * 1. 无返回值:
   *    task().then([]() { ... })
   * 2. 有返回值:
   *    task().then([]() -> NewType { return ...; })
   */
  template <typename Func>
  auto then(Func&& func) && {
    using FuncReturnType = std::invoke_result_t<Func>;

    auto prev_handle = std::exchange(handle_, nullptr);

    if constexpr (std::is_void_v<FuncReturnType>) {
      // 回调返回 void，返回 Task<void>
      return [](handle_type prev_handle, auto func) -> Task<void> {
        co_await Task<void>(prev_handle);
        LOG_TRACE("Task<void>::then - invoking then callback (void return)");
        func();
      }(prev_handle, std::forward<Func>(func));
    } else {
      // 回调返回新值，返回 Task<FuncReturnType>
      return [](handle_type prev_handle, auto func) -> Task<FuncReturnType> {
        co_await Task<void>(prev_handle);
        LOG_TRACE("Task<void>::then - invoking then callback (value return)");
        co_return func();
      }(prev_handle, std::forward<Func>(func));
    }
  }

  /**
   * @brief 捕获异常并处理，返回新的 Task<void>
   * @param func 异常处理函数
   * @return 新的 Task<void>，可以继续链式调用
   */
  Task<void> catching(std::function<void(std::exception&)>&& func) && {
    auto prev_handle = std::exchange(handle_, nullptr);
    return [](handle_type prev_handle, auto func) -> Task<void> {
      try {
        co_await Task<void>(prev_handle);
      } catch (std::exception& e) {
        LOG_TRACE("Task<void>::catching - exception caught, invoking callback");
        func(e);
        throw;  // 重新抛出异常
      }
    }(prev_handle, std::move(func));
  }

  /**
   * @brief 最终清理操作，返回新的 Task<void>
   * @param func 清理函数
   * @return 新的 Task<void>，可以继续链式调用
   */
  Task<void> finally(std::function<void()>&& func) && {
    auto prev_handle = std::exchange(handle_, nullptr);
    return [](handle_type prev_handle, auto func) -> Task<void> {
      try {
        co_await Task<void>(prev_handle);
        LOG_TRACE("Task<void>::finally - invoking finally callback (success)");
        func();
      } catch (...) {
        LOG_TRACE(
            "Task<void>::finally - invoking finally callback (exception)");
        func();
        throw;  // 重新抛出异常
      }
    }(prev_handle, std::move(func));
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

// Task<ResultType>::then 的实现必须在 Task<void> 完全定义之后
// 因为它可能返回 Task<void>
template <typename ResultType>
template <typename Func>
auto Task<ResultType>::then(Func&& func) && {
  using FuncReturnType = std::invoke_result_t<Func, ResultType>;

  auto prev_handle = std::exchange(handle_, nullptr);

  if constexpr (std::is_void_v<FuncReturnType>) {
    // 回调返回 void，返回 Task<void>
    return [](handle_type prev_handle, auto func) -> Task<void> {
      auto result = co_await Task<ResultType>(prev_handle);
      LOG_TRACE("Task::then - invoking then callback (void return)");
      func(result);
    }(prev_handle, std::forward<Func>(func));
  } else {
    // 回调返回新值，返回 Task<FuncReturnType>
    return [](handle_type prev_handle, auto func) -> Task<FuncReturnType> {
      auto result = co_await Task<ResultType>(prev_handle);
      LOG_TRACE("Task::then - invoking then callback (value return)");
      co_return func(result);
    }(prev_handle, std::forward<Func>(func));
  }
}

}  // namespace koroutine