#pragma once
#include <future>
#include <thread>

#include "../coroutine_common.h"
#include "../executors/executor.h"
#include "awaiter.hpp"

namespace koroutine {

template <typename R>
struct FutureAwaiter : public AwaiterBase<R> {
  explicit FutureAwaiter(std::future<R>&& future) noexcept
      : _future(std::move(future)) {}

  FutureAwaiter(FutureAwaiter&& awaiter) noexcept
      : AwaiterBase<R>(std::move(awaiter)),
        _future(std::move(awaiter._future)) {}

  FutureAwaiter(FutureAwaiter&) = delete;

  FutureAwaiter& operator=(FutureAwaiter&) = delete;

 protected:
  void after_suspend() override {
    std::thread([this]() { this->resume(this->_future.get()); }).detach();
  }

 private:
  std::future<R> _future;
};

}  // namespace koroutine
