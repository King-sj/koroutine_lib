#pragma once

#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"

namespace koroutine::async_io {

class AsyncSocket : public AsyncIOObject,
                    public std::enable_shared_from_this<AsyncSocket> {
 public:
  static Task<std::unique_ptr<AsyncSocket>> connect(IOEngine& engine,
                                                    const std::string& host,
                                                    uint16_t port) {
    // 平台相关的socket连接操作
    intptr_t sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to create socket");
    }
    // 省略DNS解析和连接逻辑，假设已经连接成功
    co_return std::make_unique<AsyncSocket>(engine, sockfd);
  }

  AsyncSocket(IOEngine& engine, intptr_t sockfd)
      : AsyncIOObject(engine), sockfd_(sockfd) {}

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

  intptr_t native_handle() const override { return sockfd_; }

 private:
  intptr_t sockfd_;  // Socket描述符
};
}  // namespace koroutine::async_io