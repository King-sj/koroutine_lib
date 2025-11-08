#pragma once
#include <future>
#include <thread>

#include "../coroutine_common.h"
#include "executor.h"
namespace koroutine {
class NewThreadExecutor : public AbstractExecutor {
 public:
  void execute(std::function<void()>&& func) override {
    std::thread([func = std::move(func)]() mutable { func(); }).detach();
  }
};
}  // namespace koroutine