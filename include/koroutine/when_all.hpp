#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "details/constraint.hpp"
#include "task.hpp"

namespace koroutine {

namespace details {

// Helper: 获取 Task 的 ResultType
template <typename T>
struct task_result_type;

template <typename T>
struct task_result_type<Task<T>> {
  using type = T;
};

template <typename T>
using task_result_type_t = typename task_result_type<T>::type;

// when_all 的内部实现状态
template <typename... ResultTypes>
struct WhenAllState {
  std::tuple<std::optional<ResultTypes>...> results;
  std::atomic<size_t> remaining{sizeof...(ResultTypes)};
  std::mutex mtx;
  std::condition_variable cv;
  std::vector<std::exception_ptr> exceptions;
  std::coroutine_handle<> continuation = nullptr;
  std::shared_ptr<AbstractScheduler> scheduler;
  std::vector<Task<void>> wrappers;  // 存储包装任务以延长生命周期

  bool all_completed() const { return remaining.load() == 0; }
};

}  // namespace details

/**
 * @brief 等待所有任务完成并返回所有结果的 tuple
 *
 * @tparam Tasks 任务类型参数包
 * @param tasks 要等待的任务
 * @return Task<std::tuple<ResultTypes...>> 包含所有结果的任务
 *
 * 使用示例:
 * @code
 * auto [r1, r2, r3] = co_await when_all(task1(), task2(), task3());
 * @endcode
 */
template <typename... Tasks>
  requires(details::TaskType<Tasks> && ...)
Task<std::tuple<details::task_result_type_t<Tasks>...>> when_all(
    Tasks&&... tasks) {
  using ResultTuple = std::tuple<details::task_result_type_t<Tasks>...>;

  LOG_TRACE("when_all - starting with ", sizeof...(Tasks), " tasks");

  // 创建共享状态
  auto state = std::make_shared<
      details::WhenAllState<details::task_result_type_t<Tasks>...>>();

  // 获取调度器
  state->scheduler = SchedulerManager::get_default_scheduler();
  if (!state->scheduler) {
    LOG_ERROR("when_all - no default scheduler available!");
    throw std::runtime_error("No default scheduler available for when_all");
  }
  LOG_TRACE("when_all - got scheduler: ", (void*)state->scheduler.get());
  state->wrappers.reserve(sizeof...(Tasks));

  // 为每个任务创建包装协程
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (
        [&]<std::size_t I, typename T>(Task<T>&& task) {
          LOG_TRACE("Creating wrapper for index: ", I);
          auto wrapper = [](auto s, Task<T> t) -> Task<void> {
            LOG_TRACE("wrapper for index ", I, " starting");
            try {
              LOG_TRACE("wrapper for index ", I, " awaiting task");
              T result = co_await std::move(t);
              LOG_TRACE("wrapper for index ", I,
                        " task completed successfully with result");
              {
                std::lock_guard lock(s->mtx);
                LOG_TRACE("wrapper for index ", I, " storing result at index ",
                          I);
                std::get<I>(s->results) = std::move(result);
              }
              auto remaining = s->remaining.fetch_sub(1);
              LOG_TRACE("wrapper for index ", I,
                        " remaining before decrement: ", remaining);

              if (remaining == 1) {
                // 最后一个任务完成
                LOG_TRACE("wrapper for index ", I,
                          " LAST TASK, notifying and resuming continuation");
                s->cv.notify_one();

                // 恢复 continuation
                if (s->continuation && s->scheduler) {
                  LOG_TRACE("wrapper for index ", I,
                            " scheduling continuation, handle=",
                            s->continuation.address());
                  s->scheduler->schedule(
                      ScheduleRequest(
                          s->continuation,
                          ScheduleMetadata(ScheduleMetadata::Priority::Normal,
                                           "when_all_continuation")),
                      0);
                } else {
                  LOG_ERROR("wrapper for index ", I,
                            " continuation or scheduler is null! continuation=",
                            s->continuation.address(),
                            " scheduler=", (void*)s->scheduler.get());
                }
              }
            } catch (...) {
              LOG_TRACE("when_all - task ", I, " failed with exception");
              std::lock_guard lock(s->mtx);
              s->exceptions.push_back(std::current_exception());

              if (s->remaining.fetch_sub(1) == 1) {
                s->cv.notify_one();

                if (s->continuation && s->scheduler) {
                  s->scheduler->schedule(
                      ScheduleRequest(
                          s->continuation,
                          ScheduleMetadata(ScheduleMetadata::Priority::Normal,
                                           "when_all_continuation")),
                      0);
                }
              }
            }
          };

          state->wrappers.push_back(wrapper(state, std::move(task)));
        }.template operator()<Is>(std::forward<Tasks>(tasks)),
        ...);
  }(std::index_sequence_for<Tasks...>{});

  // 启动所有包装任务
  for (auto& wrapper : state->wrappers) {
    LOG_TRACE("when_all - starting wrapper task");
    wrapper.start();
  }

  LOG_TRACE(
      "when_all - all wrapper tasks started, creating awaiter");  // 创建
                                                                  // awaiter
                                                                  // 等待所有任务完成
  struct WhenAllAwaiter {
    std::shared_ptr<
        details::WhenAllState<details::task_result_type_t<Tasks>...>>
        state;

    bool await_ready() const { return state->all_completed(); }

    void await_suspend(std::coroutine_handle<> handle) {
      LOG_TRACE(
          "WhenAllAwaiter::await_suspend - suspending until all tasks "
          "complete, handle=",
          handle.address());
      state->continuation = handle;
      LOG_TRACE(
          "WhenAllAwaiter::await_suspend - continuation set, checking if "
          "already completed");

      // 如果在设置 continuation 之前所有任务已经完成，立即恢复
      if (state->all_completed() && state->scheduler) {
        LOG_TRACE(
            "WhenAllAwaiter::await_suspend - tasks already completed, "
            "scheduling immediate resume");
        state->scheduler->schedule(
            ScheduleRequest(handle,
                            ScheduleMetadata(ScheduleMetadata::Priority::Normal,
                                             "when_all_immediate")),
            0);
      } else {
        LOG_TRACE(
            "WhenAllAwaiter::await_suspend - tasks not yet completed, "
            "waiting...");
      }
    }

    ResultTuple await_resume() {
      LOG_TRACE("WhenAllAwaiter::await_resume - all tasks completed");

      // 检查是否有异常
      if (!state->exceptions.empty()) {
        LOG_TRACE("WhenAllAwaiter::await_resume - rethrowing first exception");
        std::rethrow_exception(state->exceptions[0]);
      }

      // 构造结果 tuple
      return std::apply(
          [](auto&&... opts) { return std::make_tuple(std::move(*opts)...); },
          std::move(state->results));
    }
  };

  co_return co_await WhenAllAwaiter{state};
}

/**
 * @brief 等待所有任务完成并返回所有结果的 vector（所有任务必须返回相同类型）
 *
 * @tparam T 任务结果类型
 * @param tasks 要等待的任务向量
 * @return Task<std::vector<T>> 包含所有结果的任务
 *
 * 使用示例:
 * @code
 * std::vector<Task<int>> tasks;
 * // ... 填充 tasks
 * auto results = co_await when_all(std::move(tasks));
 * @endcode
 */
template <typename T>
Task<std::vector<T>> when_all(std::vector<Task<T>> tasks) {
  LOG_TRACE("when_all(vector) - starting with ", tasks.size(), " tasks");

  if (tasks.empty()) {
    co_return std::vector<T>{};
  }

  // 共享状态
  struct State {
    std::vector<std::optional<T>> results;
    std::atomic<size_t> remaining;
    std::mutex mtx;
    std::vector<std::exception_ptr> exceptions;
    std::coroutine_handle<> continuation = nullptr;
    std::shared_ptr<AbstractScheduler> scheduler;

    State(size_t count)
        : results(count),
          remaining(count),
          scheduler(SchedulerManager::get_default_scheduler()) {}
  };

  auto state = std::make_shared<State>(tasks.size());
  std::vector<Task<void>> wrappers;
  wrappers.reserve(tasks.size());

  // 为每个任务创建包装协程
  for (size_t i = 0; i < tasks.size(); ++i) {
    auto wrapper = [](size_t i, auto state, Task<T> task) -> Task<void> {
      try {
        T result = co_await std::move(task);
        LOG_TRACE("when_all(vector) - task ", i, " completed : ", result);
        std::lock_guard lock(state->mtx);
        state->results[i] = std::move(result);

        if (state->remaining.fetch_sub(1) == 1 && state->continuation &&
            state->scheduler) {
          state->scheduler->schedule(
              ScheduleRequest(
                  state->continuation,
                  ScheduleMetadata(ScheduleMetadata::Priority::Normal,
                                   "when_all_vec_continuation")),
              0);
        }
      } catch (...) {
        LOG_TRACE("when_all(vector) - task ", i, " failed");
        std::lock_guard lock(state->mtx);
        state->exceptions.push_back(std::current_exception());

        if (state->remaining.fetch_sub(1) == 1 && state->continuation &&
            state->scheduler) {
          state->scheduler->schedule(
              ScheduleRequest(
                  state->continuation,
                  ScheduleMetadata(ScheduleMetadata::Priority::Normal,
                                   "when_all_vec_continuation")),
              0);
        }
      }
    };

    wrappers.push_back(wrapper(i, state, std::move(tasks[i])));
  }

  // 启动所有包装任务
  for (auto& wrapper : wrappers) {
    wrapper.start();
  }

  // Awaiter
  struct WhenAllVectorAwaiter {
    std::shared_ptr<State> state;

    bool await_ready() const { return state->remaining.load() == 0; }

    void await_suspend(std::coroutine_handle<> handle) {
      LOG_TRACE("WhenAllVectorAwaiter::await_suspend");
      state->continuation = handle;

      if (state->remaining.load() == 0 && state->scheduler) {
        state->scheduler->schedule(
            ScheduleRequest(handle,
                            ScheduleMetadata(ScheduleMetadata::Priority::Normal,
                                             "when_all_vec_immediate")),
            0);
      }
    }

    std::vector<T> await_resume() {
      LOG_TRACE("WhenAllVectorAwaiter::await_resume");

      if (!state->exceptions.empty()) {
        std::rethrow_exception(state->exceptions[0]);
      }

      std::vector<T> results;
      results.reserve(state->results.size());
      for (auto& opt : state->results) {
        results.push_back(std::move(*opt));
      }
      return results;
    }
  };

  co_return co_await WhenAllVectorAwaiter{state};
}

}  // namespace koroutine
