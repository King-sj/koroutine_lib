#pragma once

#include <memory>
#include <string>
#include <system_error>

#include "koroutine/async_io/endpoint.h"
#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/io_object.h"
#include "koroutine/async_io/socket_options.h"
#include "koroutine/task.hpp"

namespace koroutine::async_io {

class AsyncSocket : public AsyncIOObject,
                    public std::enable_shared_from_this<AsyncSocket> {
 public:
  // Connect using Endpoint
  static Task<std::shared_ptr<AsyncSocket>> connect(Endpoint endpoint);
  static Task<std::shared_ptr<AsyncSocket>> connect(
      std::shared_ptr<IOEngine> engine, Endpoint endpoint);

  // Legacy/Convenience: Connect using host/port (IPv4/IPv6 auto-detection via
  // Endpoint)
  static Task<std::shared_ptr<AsyncSocket>> connect(const std::string& host,
                                                    uint16_t port);
  static Task<std::shared_ptr<AsyncSocket>> connect(
      std::shared_ptr<IOEngine> engine, const std::string& host, uint16_t port);

  AsyncSocket(std::shared_ptr<IOEngine> engine, intptr_t sockfd);
  ~AsyncSocket();

  Task<size_t> read(void* buf, size_t size) override;
  Task<size_t> write(const void* buf, size_t size) override;
  Task<void> close() override;
  void cancel();

  intptr_t native_handle() const override { return sockfd_; }

  // New features
  Endpoint local_endpoint() const;
  Endpoint remote_endpoint() const;

  // Socket Options
  template <typename Option>
  void set_option(const Option& option) {
    int level = option.level(0);
    int name = option.name(0);
    const void* data = option.data(0);
    socklen_t size = option.size(0);

#ifdef _WIN32
    if (::setsockopt(static_cast<SOCKET>(sockfd_), level, name,
                     static_cast<const char*>(data), size) == SOCKET_ERROR) {
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "setsockopt failed");
    }
#else
    if (::setsockopt(static_cast<int>(sockfd_), level, name, data, size) < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "setsockopt failed");
    }
#endif
  }

  template <typename Option>
  void get_option(Option& option) const {
    int level = option.level(0);
    int name = option.name(0);
    void* data = option.data(0);
    socklen_t size = option.size(0);

#ifdef _WIN32
    if (::getsockopt(static_cast<SOCKET>(sockfd_), level, name,
                     static_cast<char*>(data), &size) == SOCKET_ERROR) {
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "getsockopt failed");
    }
#else
    if (::getsockopt(static_cast<int>(sockfd_), level, name, data, &size) < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "getsockopt failed");
    }
#endif
    option.resize(0, size);
  }

 private:
  intptr_t sockfd_;
  bool is_closed_ = false;
};

class AsyncServerSocket
    : public AsyncIOObject,
      public std::enable_shared_from_this<AsyncServerSocket> {
 public:
  static Task<std::shared_ptr<AsyncServerSocket>> bind(Endpoint endpoint);
  static Task<std::shared_ptr<AsyncServerSocket>> bind(
      std::shared_ptr<IOEngine> engine, Endpoint endpoint);

  // Legacy/Convenience
  static Task<std::shared_ptr<AsyncServerSocket>> bind(uint16_t port);
  static Task<std::shared_ptr<AsyncServerSocket>> bind(
      std::shared_ptr<IOEngine> engine, uint16_t port);

  AsyncServerSocket(std::shared_ptr<IOEngine> engine, intptr_t sockfd);
  ~AsyncServerSocket();

  Task<std::shared_ptr<AsyncSocket>> accept();

  Task<size_t> read(void*, size_t) override;
  Task<size_t> write(const void*, size_t) override;
  Task<void> close() override;
  intptr_t native_handle() const override { return sockfd_; }

  Endpoint local_endpoint() const;

  // Socket Options
  template <typename Option>
  void set_option(const Option& option) {
    int level = option.level(0);
    int name = option.name(0);
    const void* data = option.data(0);
    socklen_t size = option.size(0);

#ifdef _WIN32
    if (::setsockopt(static_cast<SOCKET>(sockfd_), level, name,
                     static_cast<const char*>(data), size) == SOCKET_ERROR) {
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "setsockopt failed");
    }
#else
    if (::setsockopt(static_cast<int>(sockfd_), level, name, data, size) < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "setsockopt failed");
    }
#endif
  }

  template <typename Option>
  void get_option(Option& option) const {
    int level = option.level(0);
    int name = option.name(0);
    void* data = option.data(0);
    socklen_t size = option.size(0);

#ifdef _WIN32
    if (::getsockopt(static_cast<SOCKET>(sockfd_), level, name,
                     static_cast<char*>(data), &size) == SOCKET_ERROR) {
      throw std::system_error(WSAGetLastError(), std::system_category(),
                              "getsockopt failed");
    }
#else
    if (::getsockopt(static_cast<int>(sockfd_), level, name, data, &size) < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "getsockopt failed");
    }
#endif
    option.resize(0, size);
  }

 private:
  intptr_t sockfd_;
};

class AsyncUDPSocket : public AsyncIOObject,
                       public std::enable_shared_from_this<AsyncUDPSocket> {
 public:
  static Task<std::shared_ptr<AsyncUDPSocket>> create(
      IPAddress::Type type = IPAddress::Type::IPv4);
  static Task<std::shared_ptr<AsyncUDPSocket>> create(
      std::shared_ptr<IOEngine> engine,
      IPAddress::Type type = IPAddress::Type::IPv4);

  AsyncUDPSocket(std::shared_ptr<IOEngine> engine, intptr_t sockfd);
  ~AsyncUDPSocket();

  Task<void> bind(const Endpoint& endpoint);
  Task<void> connect(const Endpoint& endpoint);

  Task<size_t> send_to(const void* buf, size_t size, const Endpoint& endpoint);
  Task<std::pair<size_t, Endpoint>> recv_from(void* buf, size_t size);

  // AsyncIOObject interface
  Task<size_t> read(void* buf, size_t size) override;
  Task<size_t> write(const void* buf, size_t size) override;
  Task<void> close() override;
  intptr_t native_handle() const override { return sockfd_; }

  Endpoint local_endpoint() const;
  Endpoint remote_endpoint() const;

 private:
  intptr_t sockfd_;
};

}  // namespace koroutine::async_io
