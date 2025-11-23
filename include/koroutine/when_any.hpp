#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

#include "task.hpp"

namespace koroutine {

/**
 * @brief 等待任意一个任务完成并返回结果及其索引
 *
 * @tparam T 任务结果类型
 * @param tasks 要等待的任务向量
 * @return Task<std::pair<size_t, T>> 第一个完成的任务的索引和结果
 *
 * 使用示例:
 * @code
 * std::vector<Task<int>> tasks;
 * // ... 填充 tasks
 * auto [index, result] = co_await when_any(std::move(tasks));
 * std::cout << "Task " << index << " finished first with result: " << result <<
 * std::endl;
 * @endcode
 *
 * 注意：当第一个任务完成后，其他任务仍会继续执行（无取消机制）
 */
template <typename T>
Task<std::pair<size_t, T>> when_any(std::vector<Task<T>> tasks) {
  LOG_TRACE("when_any - starting with ", tasks.size(), " tasks");

  if (tasks.empty()) {
    throw std::invalid_argument("when_any: empty task list");
  }

  // 共享状态
  struct State {
    std::atomic<bool> completed{false};
    std::mutex mtx;
    std::optional<std::pair<size_t, T>> result;
    std::exception_ptr exception;
    std::coroutine_handle<> continuation = nullptr;
    std::shared_ptr<AbstractScheduler> scheduler;

    State() : scheduler(SchedulerManager::get_default_scheduler()) {}
  };

  auto state = std::make_shared<State>();
  std::vector<Task<void>> wrappers;
  wrappers.reserve(tasks.size());

  // 为每个任务创建包装协程
  for (size_t i = 0; i < tasks.size(); ++i) {
    auto wrapper = [state, i](Task<T> task) -> Task<void> {
      try {
        T result = co_await std::move(task);

        // 使用原子操作确保只有第一个完成的任务设置结果
        bool expected = false;
        if (state->completed.compare_exchange_strong(expected, true)) {
          LOG_TRACE("when_any - task ", i,
                    " is the first to complete successfully");
          std::lock_guard lock(state->mtx);
          state->result = std::make_pair(i, std::move(result));

          // 恢复 continuation
          if (state->continuation && state->scheduler) {
            state->scheduler->schedule(
                ScheduleRequest(
                    state->continuation,
                    ScheduleMetadata(ScheduleMetadata::Priority::High,
                                     "when_any_continuation")),
                0);
          }
        } else {
          LOG_TRACE("when_any - task ", i,
                    " completed but another task finished first");
        }
      } catch (...) {
        bool expected = false;
        if (state->completed.compare_exchange_strong(expected, true)) {
          LOG_TRACE("when_any - task ", i,
                    " is the first to complete with exception");
          std::lock_guard lock(state->mtx);
          state->exception = std::current_exception();

          if (state->continuation && state->scheduler) {
            state->scheduler->schedule(
                ScheduleRequest(
                    state->continuation,
                    ScheduleMetadata(ScheduleMetadata::Priority::High,
                                     "when_any_continuation")),
                0);
          }
        } else {
          LOG_TRACE("when_any - task ", i,
                    " failed but another task finished first");
        }
      }
    };

    wrappers.push_back(wrapper(std::move(tasks[i])));
  }

  // 启动所有包装任务
  for (auto& wrapper : wrappers) {
    wrapper.start();
  }

  // Awaiter
  struct WhenAnyAwaiter {
    std::shared_ptr<State> state;

    bool await_ready() const { return state->completed.load(); }

    void await_suspend(std::coroutine_handle<> handle) {
      LOG_TRACE("WhenAnyAwaiter::await_suspend - waiting for first task");
      state->continuation = handle;

      // 检查是否在设置 continuation 之前已经有任务完成
      if (state->completed.load() && state->scheduler) {
        state->scheduler->schedule(
            ScheduleRequest(handle,
                            ScheduleMetadata(ScheduleMetadata::Priority::High,
                                             "when_any_immediate")),
            0);
      }
    }

    std::pair<size_t, T> await_resume() {
      LOG_TRACE("WhenAnyAwaiter::await_resume - first task completed");

      std::lock_guard lock(state->mtx);

      // 检查异常
      if (state->exception) {
        LOG_TRACE("WhenAnyAwaiter::await_resume - rethrowing exception");
        std::rethrow_exception(state->exception);
      }

      // 返回结果
      if (!state->result.has_value()) {
        LOG_ERROR("WhenAnyAwaiter::await_resume - no result available!");
        throw std::runtime_error(
            "when_any: internal error - no result available");
      }

      return std::move(*state->result);
    }
  };

  co_return co_await WhenAnyAwaiter{state};
}

/**
 * @brief 等待任意一个 void 任务完成并返回其索引
 *
 * @param tasks 要等待的 void 任务向量
 * @return Task<size_t> 第一个完成的任务的索引
 */
inline Task<size_t> when_any(std::vector<Task<void>> tasks) {
  LOG_TRACE("when_any(void) - starting with ", tasks.size(), " tasks");

  if (tasks.empty()) {
    throw std::invalid_argument("when_any: empty task list");
  }

  struct State {
    std::atomic<bool> completed{false};
    std::mutex mtx;
    std::optional<size_t> result_index;
    std::exception_ptr exception;
    std::coroutine_handle<> continuation = nullptr;
    std::shared_ptr<AbstractScheduler> scheduler;

    State() : scheduler(SchedulerManager::get_default_scheduler()) {}
  };

  auto state = std::make_shared<State>();
  std::vector<Task<void>> wrappers;
  wrappers.reserve(tasks.size());

  // 为每个任务创建包装协程
  for (size_t i = 0; i < tasks.size(); ++i) {
    auto wrapper = [state, i](Task<void> task) -> Task<void> {
      try {
        co_await std::move(task);

        bool expected = false;
        if (state->completed.compare_exchange_strong(expected, true)) {
          LOG_TRACE("when_any(void) - task ", i, " is the first to complete");
          std::lock_guard lock(state->mtx);
          state->result_index = i;

          if (state->continuation && state->scheduler) {
            state->scheduler->schedule(
                ScheduleRequest(
                    state->continuation,
                    ScheduleMetadata(ScheduleMetadata::Priority::High,
                                     "when_any_void_continuation")),
                0);
          }
        }
      } catch (...) {
        bool expected = false;
        if (state->completed.compare_exchange_strong(expected, true)) {
          LOG_TRACE("when_any(void) - task ", i,
                    " is the first to complete with exception");
          std::lock_guard lock(state->mtx);
          state->exception = std::current_exception();

          if (state->continuation && state->scheduler) {
            state->scheduler->schedule(
                ScheduleRequest(
                    state->continuation,
                    ScheduleMetadata(ScheduleMetadata::Priority::High,
                                     "when_any_void_continuation")),
                0);
          }
        }
      }
    };

    wrappers.push_back(wrapper(std::move(tasks[i])));
  }

  // 启动所有包装任务
  for (auto& wrapper : wrappers) {
    wrapper.start();
  }

  struct WhenAnyVoidAwaiter {
    std::shared_ptr<State> state;

    bool await_ready() const { return state->completed.load(); }

    void await_suspend(std::coroutine_handle<> handle) {
      LOG_TRACE("WhenAnyVoidAwaiter::await_suspend");
      state->continuation = handle;

      if (state->completed.load() && state->scheduler) {
        state->scheduler->schedule(
            ScheduleRequest(handle,
                            ScheduleMetadata(ScheduleMetadata::Priority::High,
                                             "when_any_void_immediate")),
            0);
      }
    }

    size_t await_resume() {
      LOG_TRACE("WhenAnyVoidAwaiter::await_resume");

      std::lock_guard lock(state->mtx);

      if (state->exception) {
        std::rethrow_exception(state->exception);
      }

      if (!state->result_index.has_value()) {
        throw std::runtime_error(
            "when_any(void): internal error - no result available");
      }

      return *state->result_index;
    }
  };

  co_return co_await WhenAnyVoidAwaiter{state};
}

}  // namespace koroutine
