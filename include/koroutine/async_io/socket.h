#pragma once

// 平台相关的头文件
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <memory>
#include <string>
#include <system_error>

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
#ifdef _WIN32
    // Windows 平台
    SOCKET sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "Failed to create socket");
    }
    // 设置非阻塞模式
    u_long mode = 1;
    if (::ioctlsocket(sockfd, FIONBIO, &mode) != 0) {
      ::closesocket(sockfd);
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "Failed to set non-blocking mode");
    }
    intptr_t fd = static_cast<intptr_t>(sockfd);
#else
    // POSIX 平台
#if defined(__linux__)
    // Linux 支持 SOCK_NONBLOCK 标志
    intptr_t sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to create socket");
    }
#else
    // macOS 和其他 POSIX 系统使用 fcntl 设置非阻塞
    intptr_t sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to create socket");
    }
    // 设置非阻塞模式
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags == -1 || ::fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
      ::close(sockfd);
      throw std::system_error(errno, std::generic_category(),
                              "Failed to set non-blocking mode");
    }
#endif
    intptr_t fd = sockfd;
#endif
    // 省略DNS解析和连接逻辑，假设已经连接成功
    co_return std::make_unique<AsyncSocket>(engine, fd);
  }

  AsyncSocket(std::shared_ptr<IOEngine> engine, intptr_t sockfd)
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