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

struct ResolveAwaiter
    : public AwaiterBase<std::expected<std::vector<Endpoint>, int>> {
  std::string host;
  std::string service;

  ResolveAwaiter(std::string h, std::string s)
      : host(std::move(h)), service(std::move(s)) {}

 protected:
  void after_suspend() override {
    // TODO: Use NewThreadExecutor to run blocking getaddrinfo
    // In a real system, we might use a thread pool
    static auto executor = std::make_shared<NewThreadExecutor>();

    executor->execute([this, handle = this->_caller_handle]() {
      struct addrinfo hints, *res = nullptr;
      std::memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;

      int err = getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
      if (err != 0) {
        this->resume(std::unexpected(err));
        return;
      }

      std::vector<Endpoint> endpoints;
      for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        endpoints.emplace_back(p->ai_addr,
                               static_cast<socklen_t>(p->ai_addrlen));
      }

      if (res) {
        freeaddrinfo(res);
      }

      this->resume(std::move(endpoints));
    });
  }
};

Task<std::expected<std::vector<Endpoint>, int>> Resolver::resolve(
    const std::string& host, uint16_t port) {
  co_return co_await resolve(host, std::to_string(port));
}

Task<std::expected<std::vector<Endpoint>, int>> Resolver::resolve(
    const std::string& host, const std::string& service) {
  ResolveAwaiter awaiter(host, service);
  // Install the current scheduler so we can resume on it
  awaiter.install_scheduler(SchedulerManager::get_default_scheduler());
  co_return co_await std::move(awaiter);
}

}  // namespace koroutine::async_io
