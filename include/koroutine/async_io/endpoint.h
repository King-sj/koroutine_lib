#pragma once

#include <cstdint>
#include <memory>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace koroutine::async_io {

class IPAddress {
 public:
  enum class Type { IPv4, IPv6 };

  IPAddress();
  static IPAddress from_string(const std::string& ip);
  static IPAddress any(Type type = Type::IPv4);

  std::string to_string() const;
  Type type() const { return type_; }

 private:
  friend class Endpoint;
  Type type_;
  union {
    struct in_addr v4;
    struct in6_addr v6;
  } addr_;
};

class Endpoint {
 public:
  Endpoint() = default;
  Endpoint(const IPAddress& addr, uint16_t port);
  Endpoint(const std::string& ip, uint16_t port);
  Endpoint(const struct sockaddr* addr, socklen_t len);

  IPAddress address() const;
  uint16_t port() const;
  std::string to_string() const;

  // Low-level access
  struct sockaddr* data();
  const struct sockaddr* data() const;
  socklen_t length() const;
  int family() const;

 private:
  struct sockaddr_storage storage_{};
  socklen_t length_{0};
};

}  // namespace koroutine::async_io
