#pragma once
#include "awaiter.hpp"
#include "koroutine/async_io/op.h"

namespace koroutine::async_io {
template <typename T>
struct IOAwaiter : public AwaiterBase<T> {
  std::shared_ptr<AsyncIOOp> io_op;
  explicit IOAwaiter(std::shared_ptr<AsyncIOOp> op) noexcept
      : io_op(std::move(op)) {}
  IOAwaiter(IOAwaiter&& other) noexcept
      : AwaiterBase<T>(std::move(other)), io_op(std::move(other.io_op)) {
    LOG_INFO("IOAwaiter::move constructor - moved IOAwaiter");
  }

  IOAwaiter(IOAwaiter&) = delete;

  IOAwaiter& operator=(IOAwaiter&) = delete;

 protected:
  void after_suspend() override {
    LOG_INFO("IOAwaiter::after_suspend - IO operation submitted");
    io_op->coro_handle = this->_caller_handle;
    io_op->io_object->engine_->submit(io_op);
  }

  void before_resume() override {
    LOG_INFO("IOAwaiter::before_resume - IO operation completed");
    if (io_op->error) {
      throw std::system_error(io_op->error);
    }
    if constexpr (std::is_same_v<T, void>) {
      this->_result = Result<void>();
    } else if constexpr (std::is_same_v<T, size_t>) {
      this->_result = Result<size_t>(std::move(io_op->actual_size));
    } else {
      this->_result = Result<T>(std::move(*static_cast<T*>(io_op->buffer)));
    }
  }
};
}  // namespace koroutine::async_io
