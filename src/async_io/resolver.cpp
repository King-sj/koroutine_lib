#include "koroutine/async_io/resolver.h"

#include <cstring>
#include <system_error>
#include <thread>

#include "koroutine/awaiters/awaiter.hpp"
#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/scheduler_manager.h"
#include "koroutine/schedulers/schedule_request.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace koroutine::async_io {

Task<std::expected<std::vector<Endpoint>, int>> Resolver::resolve(
    const std::string& host, uint16_t port) {
  co_return co_await resolve(host, std::to_string(port));
}

Task<std::expected<std::vector<Endpoint>, int>> Resolver::resolve(
    const std::string& host, const std::string& service) {
  struct addrinfo hints, *res = nullptr;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;  // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;

  // fprintf(stderr, "DEBUG: Resolving %s:%s\n", host.c_str(),
  // service.c_str());
  // TODO: Offload to thread pool to avoid blocking event loop
  int err = getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
  if (err != 0) {
    co_return std::unexpected(err);
  }
  std::vector<Endpoint> result;
  for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
    result.emplace_back(p->ai_addr, static_cast<socklen_t>(p->ai_addrlen));
  }
  if (res) {
    freeaddrinfo(res);
  }
  co_return result;
}

}  // namespace koroutine::async_io
