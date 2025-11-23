#pragma once

#include <memory>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "koroutine/async_io/io_object.h"
#include "koroutine/async_io/op.h"
#include "koroutine/awaiters/io_awaiter.hpp"

namespace koroutine::async_io {

class AsyncStandardStream
    : public AsyncIOObject,
      public std::enable_shared_from_this<AsyncStandardStream> {
 public:
  AsyncStandardStream(std::shared_ptr<IOEngine> engine, int fd)
      : AsyncIOObject(std::move(engine)), fd_(fd) {
#ifndef _WIN32
    // 设置非阻塞模式
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags != -1) {
      ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    }
#endif
  }

  Task<size_t> read(void* buf, size_t size) override {
    auto io_op = std::make_shared<AsyncIOOp>(OpType::READ, shared_from_this(),
                                             buf, size);
    co_return co_await IOAwaiter<size_t>{io_op};
  }

  Task<size_t> write(const void* buf, size_t size) override {
    auto io_op = std::make_shared<AsyncIOOp>(OpType::WRITE, shared_from_this(),
                                             const_cast<void*>(buf), size);
    co_return co_await IOAwaiter<size_t>{io_op};
  }

  Task<void> close() override {
    // 标准流不关闭底层 fd
    co_return;
  }

  intptr_t native_handle() const override { return fd_; }

 private:
  int fd_;
};

inline std::shared_ptr<AsyncStandardStream> async_stdin(
    std::shared_ptr<IOEngine> engine) {
#ifdef _WIN32
  return std::make_shared<AsyncStandardStream>(engine, 0);
#else
  return std::make_shared<AsyncStandardStream>(engine, STDIN_FILENO);
#endif
}

inline std::shared_ptr<AsyncStandardStream> async_stdout(
    std::shared_ptr<IOEngine> engine) {
#ifdef _WIN32
  return std::make_shared<AsyncStandardStream>(engine, 1);
#else
  return std::make_shared<AsyncStandardStream>(engine, STDOUT_FILENO);
#endif
}

inline std::shared_ptr<AsyncStandardStream> async_stderr(
    std::shared_ptr<IOEngine> engine) {
#ifdef _WIN32
  return std::make_shared<AsyncStandardStream>(engine, 2);
#else
  return std::make_shared<AsyncStandardStream>(engine, STDERR_FILENO);
#endif
}

}  // namespace koroutine::async_io
