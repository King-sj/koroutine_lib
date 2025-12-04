#include "koroutine/async_io/endpoint.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace koroutine::async_io {

// --- IPAddress ---

IPAddress::IPAddress() : type_(Type::IPv4) {
  std::memset(&addr_, 0, sizeof(addr_));
}

IPAddress IPAddress::from_string(const std::string& ip) {
  IPAddress result;
  if (inet_pton(AF_INET, ip.c_str(), &result.addr_.v4) == 1) {
    result.type_ = Type::IPv4;
    return result;
  }
  if (inet_pton(AF_INET6, ip.c_str(), &result.addr_.v6) == 1) {
    result.type_ = Type::IPv6;
    return result;
  }
  throw std::invalid_argument("Invalid IP address format");
}

IPAddress IPAddress::any(Type type) {
  IPAddress result;
  result.type_ = type;
  std::memset(&result.addr_, 0,
              sizeof(result.addr_));  // 0 is ANY for both v4 and v6
  return result;
}

std::string IPAddress::to_string() const {
  char buf[INET6_ADDRSTRLEN];
  if (type_ == Type::IPv4) {
    if (inet_ntop(AF_INET, &addr_.v4, buf, sizeof(buf))) {
      return std::string(buf);
    }
  } else {
    if (inet_ntop(AF_INET6, &addr_.v6, buf, sizeof(buf))) {
      return std::string(buf);
    }
  }
  return "";
}

// --- Endpoint ---

Endpoint::Endpoint(const IPAddress& addr, uint16_t port) {
  std::memset(&storage_, 0, sizeof(storage_));
  if (addr.type() == IPAddress::Type::IPv4) {
    struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(&storage_);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr = addr.addr_.v4;
    length_ = sizeof(struct sockaddr_in);
    // printf("DEBUG: Endpoint IPv4 family set to %d (AF_INET=%d)\n",
    // sin->sin_family, AF_INET);
  } else {
    struct sockaddr_in6* sin6 =
        reinterpret_cast<struct sockaddr_in6*>(&storage_);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    sin6->sin6_addr = addr.addr_.v6;
    length_ = sizeof(struct sockaddr_in6);
  }
}

Endpoint::Endpoint(const std::string& ip, uint16_t port)
    : Endpoint(IPAddress::from_string(ip), port) {}

Endpoint::Endpoint(const struct sockaddr* addr, socklen_t len) {
  std::memset(&storage_, 0, sizeof(storage_));
  if (len > sizeof(storage_)) {
    // Should not happen for supported families
    len = sizeof(storage_);
  }
  std::memcpy(&storage_, addr, len);
  length_ = len;
}

IPAddress Endpoint::address() const {
  IPAddress result;
  if (storage_.ss_family == AF_INET) {
    result.type_ = IPAddress::Type::IPv4;
    const struct sockaddr_in* sin =
        reinterpret_cast<const struct sockaddr_in*>(&storage_);
    result.addr_.v4 = sin->sin_addr;
  } else if (storage_.ss_family == AF_INET6) {
    result.type_ = IPAddress::Type::IPv6;
    const struct sockaddr_in6* sin6 =
        reinterpret_cast<const struct sockaddr_in6*>(&storage_);
    result.addr_.v6 = sin6->sin6_addr;
  }
  return result;
}

uint16_t Endpoint::port() const {
  if (storage_.ss_family == AF_INET) {
    const struct sockaddr_in* sin =
        reinterpret_cast<const struct sockaddr_in*>(&storage_);
    return ntohs(sin->sin_port);
  } else if (storage_.ss_family == AF_INET6) {
    const struct sockaddr_in6* sin6 =
        reinterpret_cast<const struct sockaddr_in6*>(&storage_);
    return ntohs(sin6->sin6_port);
  }
  return 0;
}

std::string Endpoint::to_string() const {
  return address().to_string() + ":" + std::to_string(port());
}

struct sockaddr* Endpoint::data() {
  return reinterpret_cast<struct sockaddr*>(&storage_);
}

const struct sockaddr* Endpoint::data() const {
  return reinterpret_cast<const struct sockaddr*>(&storage_);
}

socklen_t Endpoint::length() const { return length_; }

int Endpoint::family() const { return storage_.ss_family; }

}  // namespace koroutine::async_io
