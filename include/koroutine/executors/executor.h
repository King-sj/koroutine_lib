#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "koroutine/debug.h"
namespace koroutine {

class AbstractExecutor {
 public:
  virtual ~AbstractExecutor() = default;

  // execute immediately / enqueue for execution
  virtual void execute(std::function<void()>&& func) = 0;

  // execute after delay (ms). Default implementation uses a detached thread
  // to sleep then call execute(). Implementations may override for better
  // timer integration.
  virtual void execute_delayed(std::function<void()>&& func, long long ms) {
    std::thread([this, f = std::move(func), ms]() mutable {
      if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      this->execute(std::move(f));
    }).detach();
  }
};

}  // namespace koroutine