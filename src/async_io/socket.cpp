#include "koroutine/async_io/socket.h"

#include "koroutine/async_io/resolver.h"
#include "koroutine/awaiters/io_awaiter.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>

namespace koroutine::async_io {

namespace {
void set_nonblocking(intptr_t fd) {
#ifdef _WIN32
  u_long mode = 1;
  if (::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode) != 0) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Failed to set non-blocking mode");
  }
#else
  int flags = ::fcntl(static_cast<int>(fd), F_GETFL, 0);
  if (flags == -1 ||
      ::fcntl(static_cast<int>(fd), F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set non-blocking mode");
  }
#endif
}

void close_socket(intptr_t fd) {
#ifdef _WIN32
  ::closesocket(static_cast<SOCKET>(fd));
#else
  ::close(static_cast<int>(fd));
#endif
}
}  // namespace

// --- AsyncSocket ---

AsyncSocket::AsyncSocket(std::shared_ptr<IOEngine> engine, intptr_t sockfd)
    : AsyncIOObject(engine), sockfd_(sockfd) {}

AsyncSocket::~AsyncSocket() {
  if (!is_closed_ && sockfd_ != -1) {
    close_socket(sockfd_);
  }
}

Task<std::shared_ptr<AsyncSocket>> AsyncSocket::connect(Endpoint endpoint) {
  return connect(get_default_io_engine(), endpoint);
}

Task<std::shared_ptr<AsyncSocket>> AsyncSocket::connect(
    std::shared_ptr<IOEngine> engine, Endpoint endpoint) {
  int family = endpoint.family();
  int type = SOCK_STREAM;
  int protocol = 0;  // IPPROTO_TCP

#ifdef _WIN32
  SOCKET sockfd = ::socket(family, type, protocol);
  if (sockfd == INVALID_SOCKET) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Failed to create socket");
  }
  intptr_t fd = static_cast<intptr_t>(sockfd);

  // Bind is required for ConnectEx
  struct sockaddr_storage bind_addr;
  std::memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.ss_family = family;
  int bind_len =
      (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

  if (::bind(sockfd, (struct sockaddr*)&bind_addr, bind_len) != 0) {
    close_socket(fd);
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Bind failed");
  }

  auto socket = std::make_shared<AsyncSocket>(engine, fd);
  auto io_op = std::make_shared<AsyncIOOp>(
      OpType::CONNECT, socket->shared_from_this(), nullptr, 0);

  std::memcpy(&io_op->addr, endpoint.data(), endpoint.length());
  io_op->addr_len = endpoint.length();

  co_await IOAwaiter<void>{io_op};

  // Update connect context
  if (setsockopt(sockfd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) != 0) {
    // Ignore error
  }

  co_return socket;

#else
#if defined(__linux__)
  intptr_t sockfd = ::socket(family, type | SOCK_NONBLOCK, protocol);
#else
  intptr_t sockfd = ::socket(family, type, protocol);
#endif
  if (sockfd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create socket");
  }
  intptr_t fd = sockfd;

#if !defined(__linux__)
  try {
    set_nonblocking(fd);
  } catch (...) {
    close_socket(fd);
    throw;
  }
#endif

  // Connect
  int ret = ::connect(static_cast<int>(fd), endpoint.data(), endpoint.length());
  if (ret == 0) {
    co_return std::make_shared<AsyncSocket>(engine, fd);
  } else {
    if (errno != EINPROGRESS) {
      close_socket(fd);
      throw std::system_error(errno, std::generic_category(), "Connect failed");
    }
  }

  auto socket = std::make_shared<AsyncSocket>(engine, fd);
  auto io_op = std::make_shared<AsyncIOOp>(
      OpType::CONNECT, socket->shared_from_this(), nullptr, 0);
  co_await IOAwaiter<void>{io_op};

  // Check error
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
#endif
}

Task<std::shared_ptr<AsyncSocket>> AsyncSocket::connect(const std::string& host,
                                                        uint16_t port) {
  return connect(get_default_io_engine(), host, port);
}

Task<std::shared_ptr<AsyncSocket>> AsyncSocket::connect(
    std::shared_ptr<IOEngine> engine, const std::string& host, uint16_t port) {
  // Try to parse as IP literal first
  std::optional<Endpoint> ep;
  try {
    ep = Endpoint(host, port);
  } catch (const std::invalid_argument&) {
    // Not an IP literal, try DNS resolution
  }

  if (ep) {
    co_return co_await connect(engine, *ep);
  }

  // Not an IP literal, try DNS resolution
  auto result = co_await Resolver::resolve(host, port);
  if (!result) {
    throw std::runtime_error("Could not resolve host: " + host +
                             ", error: " + std::to_string(result.error()));
  }
  auto endpoints = std::move(result.value());
  if (endpoints.empty()) {
    throw std::runtime_error("Could not resolve host: " + host);
  }

  // fprintf(stderr, "DEBUG: Resolved %zu endpoints for %s\n", endpoints.size(),
  // host.c_str());

  // Try each endpoint
  std::exception_ptr last_exception;
  for (const auto& ep : endpoints) {
    try {
      // fprintf(stderr, "DEBUG: Trying to connect to %s\n",
      // ep.to_string().c_str());
      co_return co_await connect(engine, ep);
    } catch (...) {
      // fprintf(stderr, "DEBUG: Failed to connect to %s\n",
      // ep.to_string().c_str());
      last_exception = std::current_exception();
    }
  }
  if (last_exception) {
    std::rethrow_exception(last_exception);
  }
  throw std::runtime_error("Failed to connect to any resolved address for " +
                           host);
}

Task<size_t> AsyncSocket::read(void* buf, size_t size) {
  LOG_TRACE("AsyncSocket::read fd=", (long)sockfd_, " size=", size);
  auto io_op =
      std::make_shared<AsyncIOOp>(OpType::READ, shared_from_this(), buf, size);
  co_return co_await IOAwaiter<size_t>{io_op};
}

Task<size_t> AsyncSocket::write(const void* buf, size_t size) {
  LOG_TRACE("AsyncSocket::write fd=", (long)sockfd_, " size=", size);
  auto io_op = std::make_shared<AsyncIOOp>(OpType::WRITE, shared_from_this(),
                                           const_cast<void*>(buf), size);
  co_return co_await IOAwaiter<size_t>{io_op};
}

Task<void> AsyncSocket::close() {
  if (is_closed_) co_return;
  is_closed_ = true;
  auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                           nullptr, 0);
  co_await IOAwaiter<void>{io_op};
}

void AsyncSocket::cancel() {
  if (is_closed_) return;
  is_closed_ = true;
  auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                           nullptr, 0);
  engine_->submit(io_op);
}

Endpoint AsyncSocket::local_endpoint() const {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getsockname(static_cast<int>(sockfd_), (struct sockaddr*)&addr, &len) <
      0) {
    throw std::system_error(errno, std::generic_category(),
                            "getsockname failed");
  }
  return Endpoint((struct sockaddr*)&addr, len);
}

Endpoint AsyncSocket::remote_endpoint() const {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getpeername(static_cast<int>(sockfd_), (struct sockaddr*)&addr, &len) <
      0) {
    throw std::system_error(errno, std::generic_category(),
                            "getpeername failed");
  }
  return Endpoint((struct sockaddr*)&addr, len);
}

// --- AsyncServerSocket ---

AsyncServerSocket::AsyncServerSocket(std::shared_ptr<IOEngine> engine,
                                     intptr_t sockfd)
    : AsyncIOObject(engine), sockfd_(sockfd) {}

AsyncServerSocket::~AsyncServerSocket() {}

Task<std::shared_ptr<AsyncServerSocket>> AsyncServerSocket::bind(
    Endpoint endpoint) {
  return bind(get_default_io_engine(), endpoint);
}

Task<std::shared_ptr<AsyncServerSocket>> AsyncServerSocket::bind(
    std::shared_ptr<IOEngine> engine, Endpoint endpoint) {
  int family = endpoint.family();
  int type = SOCK_STREAM;
  int protocol = 0;

#ifdef _WIN32
  SOCKET sockfd = ::socket(family, type, protocol);
  if (sockfd == INVALID_SOCKET) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Failed to create socket");
  }
  intptr_t fd = static_cast<intptr_t>(sockfd);
#else
  intptr_t sockfd = ::socket(family, type, protocol);
  if (sockfd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create socket");
  }
  intptr_t fd = sockfd;
#endif

  try {
    set_nonblocking(fd);
  } catch (...) {
    close_socket(fd);
    throw;
  }

  int opt = 1;
#ifdef _WIN32
  ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));
#else
  ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_REUSEADDR, &opt,
               sizeof(opt));
#endif

  if (::bind(static_cast<int>(fd), endpoint.data(), endpoint.length()) < 0) {
    close_socket(fd);
#ifdef _WIN32
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Bind failed");
#else
    throw std::system_error(errno, std::generic_category(), "Bind failed");
#endif
  }

  if (::listen(static_cast<int>(fd), SOMAXCONN) < 0) {
    close_socket(fd);
#ifdef _WIN32
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Listen failed");
#else
    throw std::system_error(errno, std::generic_category(), "Listen failed");
#endif
  }

  co_return std::make_shared<AsyncServerSocket>(engine, fd);
}

Task<std::shared_ptr<AsyncServerSocket>> AsyncServerSocket::bind(
    uint16_t port) {
  return bind(get_default_io_engine(), port);
}

Task<std::shared_ptr<AsyncServerSocket>> AsyncServerSocket::bind(
    std::shared_ptr<IOEngine> engine, uint16_t port) {
  // Default to IPv4 ANY
  return bind(engine, Endpoint(IPAddress::any(IPAddress::Type::IPv4), port));
}

Task<std::shared_ptr<AsyncSocket>> AsyncServerSocket::accept() {
  struct sockaddr_storage client_addr;
  socklen_t client_len = sizeof(client_addr);

  auto io_op = std::make_shared<AsyncIOOp>(OpType::ACCEPT, shared_from_this(),
                                           &client_addr, client_len);

  size_t new_fd = co_await IOAwaiter<size_t>{io_op};

  co_return std::make_shared<AsyncSocket>(engine_,
                                          static_cast<intptr_t>(new_fd));
}

Task<size_t> AsyncServerSocket::read(void*, size_t) {
  throw std::runtime_error("Cannot read from server socket");
  co_return 0;
}

Task<size_t> AsyncServerSocket::write(const void*, size_t) {
  throw std::runtime_error("Cannot write to server socket");
  co_return 0;
}

Task<void> AsyncServerSocket::close() {
  auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                           nullptr, 0);
  co_await IOAwaiter<void>{io_op};
}

Endpoint AsyncServerSocket::local_endpoint() const {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getsockname(static_cast<int>(sockfd_), (struct sockaddr*)&addr, &len) <
      0) {
    throw std::system_error(errno, std::generic_category(),
                            "getsockname failed");
  }
  return Endpoint((struct sockaddr*)&addr, len);
}

// --- AsyncUDPSocket ---

Task<std::shared_ptr<AsyncUDPSocket>> AsyncUDPSocket::create(
    IPAddress::Type type) {
  return create(get_default_io_engine(), type);
}

Task<std::shared_ptr<AsyncUDPSocket>> AsyncUDPSocket::create(
    std::shared_ptr<IOEngine> engine, IPAddress::Type type) {
  int domain = (type == IPAddress::Type::IPv4) ? AF_INET : AF_INET6;
  int sock_type = SOCK_DGRAM;
  int protocol = 0;

#ifdef _WIN32
  SOCKET sockfd = ::socket(domain, sock_type, protocol);
  if (sockfd == INVALID_SOCKET) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Failed to create socket");
  }
  intptr_t fd = static_cast<intptr_t>(sockfd);
#else
  intptr_t fd = ::socket(domain, sock_type, protocol);
  if (fd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create socket");
  }
#endif

  try {
    set_nonblocking(fd);
  } catch (...) {
    close_socket(fd);
    throw;
  }

  co_return std::make_shared<AsyncUDPSocket>(engine, fd);
}

AsyncUDPSocket::AsyncUDPSocket(std::shared_ptr<IOEngine> engine,
                               intptr_t sockfd)
    : AsyncIOObject(engine), sockfd_(sockfd) {}

AsyncUDPSocket::~AsyncUDPSocket() {
  // User should call close()
}

Task<void> AsyncUDPSocket::bind(const Endpoint& endpoint) {
  if (::bind(static_cast<int>(sockfd_), endpoint.data(), endpoint.length()) <
      0) {
#ifdef _WIN32
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Bind failed");
#else
    throw std::system_error(errno, std::generic_category(), "Bind failed");
#endif
  }
  co_return;
}

Task<void> AsyncUDPSocket::connect(const Endpoint& endpoint) {
  if (::connect(static_cast<int>(sockfd_), endpoint.data(), endpoint.length()) <
      0) {
#ifdef _WIN32
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "Connect failed");
#else
    throw std::system_error(errno, std::generic_category(), "Connect failed");
#endif
  }
  co_return;
}

Task<size_t> AsyncUDPSocket::send_to(const void* buf, size_t size,
                                     const Endpoint& endpoint) {
  auto io_op = std::make_shared<AsyncIOOp>(OpType::SENDTO, shared_from_this(),
                                           const_cast<void*>(buf), size);

  if (endpoint.length() > sizeof(io_op->addr)) {
    throw std::invalid_argument("Endpoint address too large");
  }
  std::memcpy(&io_op->addr, endpoint.data(), endpoint.length());
  io_op->addr_len = endpoint.length();

  co_return co_await IOAwaiter<size_t>{io_op};
}

Task<std::pair<size_t, Endpoint>> AsyncUDPSocket::recv_from(void* buf,
                                                            size_t size) {
  auto io_op = std::make_shared<AsyncIOOp>(OpType::RECVFROM, shared_from_this(),
                                           buf, size);

  size_t n = co_await IOAwaiter<size_t>{io_op};

  Endpoint ep((struct sockaddr*)&io_op->addr, io_op->addr_len);
  co_return std::make_pair(n, ep);
}

Task<size_t> AsyncUDPSocket::read(void* buf, size_t size) {
  auto io_op =
      std::make_shared<AsyncIOOp>(OpType::READ, shared_from_this(), buf, size);
  co_return co_await IOAwaiter<size_t>{io_op};
}

Task<size_t> AsyncUDPSocket::write(const void* buf, size_t size) {
  auto io_op = std::make_shared<AsyncIOOp>(OpType::WRITE, shared_from_this(),
                                           const_cast<void*>(buf), size);
  co_return co_await IOAwaiter<size_t>{io_op};
}

Task<void> AsyncUDPSocket::close() {
  auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                           nullptr, 0);
  co_await IOAwaiter<void>{io_op};
}

Endpoint AsyncUDPSocket::local_endpoint() const {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getsockname(static_cast<int>(sockfd_), (struct sockaddr*)&addr, &len) <
      0) {
    throw std::system_error(errno, std::generic_category(),
                            "getsockname failed");
  }
  return Endpoint((struct sockaddr*)&addr, len);
}

Endpoint AsyncUDPSocket::remote_endpoint() const {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (::getpeername(static_cast<int>(sockfd_), (struct sockaddr*)&addr, &len) <
      0) {
    throw std::system_error(errno, std::generic_category(),
                            "getpeername failed");
  }
  return Endpoint((struct sockaddr*)&addr, len);
}

}  // namespace koroutine::async_io
