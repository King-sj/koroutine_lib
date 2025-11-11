#pragma once
#include <future>
#include <thread>

#include "../coroutine_common.h"
#include "executor.h"
namespace koroutine {
class NewThreadExecutor : public AbstractExecutor {
 public:
  void execute(std::function<void()>&& func) override {
    LOG_TRACE("NewThreadExecutor::execute - launching new thread");
    std::thread([func = std::move(func)]() mutable { func(); }).detach();
    LOG_TRACE("NewThreadExecutor::execute - thread launched");
  }
};
}  // namespace koroutine