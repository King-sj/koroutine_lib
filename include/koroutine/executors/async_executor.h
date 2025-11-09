#pragma once
#include <chrono>
#include <future>

#include "../coroutine_common.h"
#include "executor.h"
namespace koroutine {
class AsyncExecutor : public AbstractExecutor {
 public:
  void execute(std::function<void()>&& func) override {
    std::unique_lock lock(future_lock);
    LOG_DEBUG("AsyncExecutor::execute called.");
    auto id = nextId++;
    lock.unlock();

    std::async(std::launch::async,
               [this, func = std::move(func), id]() mutable {
                 func();
                 std::unique_lock lock(future_lock);
                 futures.erase(id);
               });
  }

  void execute_delayed(std::function<void()>&& func, long long ms) override {
    std::unique_lock lock(future_lock);
    LOG_DEBUG("AsyncExecutor::execute_delayed called.", ms);
    auto id = nextId++;
    lock.unlock();

    std::async(std::launch::async,
               [this, func = std::move(func), id, ms]() mutable {
                 // FIXME: below will block a thread, should just block
                 // coroutine
                 if (ms > 0)
                   std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                 func();
                 std::unique_lock lock(future_lock);
                 futures.erase(id);
               });
  }

 private:
  std::mutex future_lock;
  std::unordered_map<int, std::future<void>> futures;
  int nextId = 0;
};
}  // namespace koroutine
