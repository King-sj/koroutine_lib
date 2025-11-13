#pragma once

#include <memory>

#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"
namespace koroutine::async_io {

class AsyncFile : public AsyncIOObject,
                  public std::enable_shared_from_this<AsyncFile> {
 public:
  static Task<std::unique_ptr<AsyncFile>> open(IOEngine& engine,
                                               const std::string& path,
                                               std::ios::openmode mode) {
    // 平台相关的文件打开操作
    intptr_t fd = ::open(path.c_str(), translate_mode(mode) | O_NONBLOCK);
    if (fd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to open file");
    }
    co_return std::make_unique<AsyncFile>(engine, fd);
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
    auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                             nullptr, 0);
    co_await IOAwaiter<void>{io_op};
  }

  intptr_t native_handle() const override { return fd_; }

  Task<void> seek(size_t position) {
    // 模拟seek操作
    co_return;
  }

  Task<void> flush() {
    // 模拟flush操作
    co_return;
  }

 private:
  intptr_t fd_;  // 文件描述符
};
}  // namespace koroutine::async_io
