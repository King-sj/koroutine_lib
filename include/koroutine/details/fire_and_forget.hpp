#pragma once

#include <coroutine>
#include <exception>
#include <utility>

#include "../awaiters/task_awaiter.hpp"
#include "../scheduler_manager.h"

namespace koroutine {

// Forward declaration
template <typename T>
class Task;

namespace details {

struct FireAndForget {
  struct promise_type {
    FireAndForget get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() { std::terminate(); }

    // 支持 co_await Task<T>
    template <typename T>
    auto await_transform(Task<T>&& task) {
      auto awaiter = TaskAwaiter<T>(std::move(task));
      // FireAndForget 协程通常没有绑定的调度器，使用默认调度器
      awaiter.install_scheduler(SchedulerManager::get_default_scheduler());
      return awaiter;
    }

    // 支持其他 Awaitable
    template <typename Awaitable>
    decltype(auto) await_transform(Awaitable&& awaitable) {
      return std::forward<Awaitable>(awaitable);
    }
  };
};

}  // namespace details
}  // namespace koroutine
