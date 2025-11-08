#pragma once
#include <future>

#include "../coroutine_common.h"
#include "executor.h"
namespace koroutine {
class AsyncExecutor : public AbstractExecutor {
 public:
  void execute(std::function<void()>&& func) override {
    std::unique_lock lock(future_lock);
    auto id = nextId++;
    lock.unlock();

    std::async(std::launch::async,
               [this, func = std::move(func), id]() mutable {
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
