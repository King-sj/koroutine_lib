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

#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"
#include "koroutine/task.hpp"

namespace koroutine::async_io {

class AsyncSocket : public AsyncIOObject,
                    public std::enable_shared_from_this<AsyncSocket> {
 public:
  static Task<std::shared_ptr<AsyncSocket>> connect(const std::string& host,
                                                    uint16_t port) {
    return connect(get_default_io_engine(), host, port);
  }

  static Task<std::shared_ptr<AsyncSocket>> connect(
      std::shared_ptr<IOEngine> engine, const std::string& host,
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

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      // 简单的DNS解析尝试 (仅示例，实际应使用 getaddrinfo)
      // 这里假设 host 是 IP 地址
      throw std::system_error(errno, std::generic_category(),
                              "Invalid address");
    }

    int ret =
        ::connect(static_cast<int>(fd), (struct sockaddr*)&addr, sizeof(addr));
    if (ret == 0) {
      // 立即连接成功
      co_return std::make_shared<AsyncSocket>(engine, fd);
    } else {
#ifdef _WIN32
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        ::closesocket(static_cast<SOCKET>(fd));
        throw std::system_error(WSAGetLastError(), std::system_category(),
                                "Connect failed");
      }
#else
      if (errno != EINPROGRESS) {
        ::close(static_cast<int>(fd));
        throw std::system_error(errno, std::generic_category(),
                                "Connect failed");
      }
#endif
    }

    auto socket = std::make_shared<AsyncSocket>(engine, fd);
    // 等待连接完成
    auto io_op = std::make_shared<AsyncIOOp>(
        OpType::CONNECT, socket->shared_from_this(), nullptr, 0);
    co_await IOAwaiter<void>{io_op};

    // 检查连接错误
    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_ERROR, (char*)&error,
                     &len) < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "getsockopt failed");
    }
    if (error != 0) {
      throw std::system_error(error, std::generic_category(),
                              "Connect failed asynchronously");
    }

    co_return socket;
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

class AsyncServerSocket
    : public AsyncIOObject,
      public std::enable_shared_from_this<AsyncServerSocket> {
 public:
  static Task<std::shared_ptr<AsyncServerSocket>> bind(uint16_t port) {
    return bind(get_default_io_engine(), port);
  }

  static Task<std::shared_ptr<AsyncServerSocket>> bind(
      std::shared_ptr<IOEngine> engine, uint16_t port) {
#ifdef _WIN32
    SOCKET sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "Failed to create socket");
    }
    u_long mode = 1;
    if (::ioctlsocket(sockfd, FIONBIO, &mode) != 0) {
      ::closesocket(sockfd);
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "Failed to set non-blocking mode");
    }
    intptr_t fd = static_cast<intptr_t>(sockfd);
#else
    intptr_t sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to create socket");
    }
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags == -1 || ::fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
      ::close(sockfd);
      throw std::system_error(errno, std::generic_category(),
                              "Failed to set non-blocking mode");
    }
    intptr_t fd = sockfd;
#endif

    int opt = 1;
#ifdef _WIN32
    ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_REUSEADDR,
                 (const char*)&opt, sizeof(opt));
#else
    ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_REUSEADDR, &opt,
                 sizeof(opt));
#endif

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(static_cast<int>(fd), (struct sockaddr*)&addr, sizeof(addr)) <
        0) {
#ifdef _WIN32
      ::closesocket(static_cast<SOCKET>(fd));
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "Bind failed");
#else
      ::close(static_cast<int>(fd));
      throw std::system_error(errno, std::generic_category(), "Bind failed");
#endif
    }

    if (::listen(static_cast<int>(fd), SOMAXCONN) < 0) {
#ifdef _WIN32
      ::closesocket(static_cast<SOCKET>(fd));
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "Listen failed");
#else
      ::close(static_cast<int>(fd));
      throw std::system_error(errno, std::generic_category(), "Listen failed");
#endif
    }

    co_return std::make_shared<AsyncServerSocket>(engine, fd);
  }

  AsyncServerSocket(std::shared_ptr<IOEngine> engine, intptr_t sockfd)
      : AsyncIOObject(engine), sockfd_(sockfd) {}

  Task<std::shared_ptr<AsyncSocket>> accept() {
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    auto io_op = std::make_shared<AsyncIOOp>(OpType::ACCEPT, shared_from_this(),
                                             &client_addr, client_len);
    // ACCEPT returns the new fd in actual_size (hacky but works for now)
    // Or we can rely on the engine to populate something.
    // Let's assume the engine will put the new fd in `actual_size`.

    size_t new_fd = co_await IOAwaiter<size_t>{io_op};

    co_return std::make_shared<AsyncSocket>(engine_,
                                            static_cast<intptr_t>(new_fd));
  }

  // Implement pure virtuals
  Task<size_t> read(void*, size_t) override {
    throw std::runtime_error("Cannot read from server socket");
    co_return 0;
  }
  Task<size_t> write(const void*, size_t) override {
    throw std::runtime_error("Cannot write to server socket");
    co_return 0;
  }
  Task<void> close() override {
    auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                             nullptr, 0);
    co_await IOAwaiter<void>{io_op};
  }
  intptr_t native_handle() const override { return sockfd_; }

 private:
  intptr_t sockfd_;
};
}  // namespace koroutine::async_io