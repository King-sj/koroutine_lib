#include "koroutine/async_io/engin.h"

#include <stdexcept>

#ifdef __APPLE__
#include "koroutine/async_io/platform/macos/kqueue_engin.h"
#elif defined(__linux__)
// #include "koroutine/async_io/platform/linux/epoll_engin.h"
#elif defined(_WIN32)
// #include "koroutine/async_io/platform/win/iocp_engin.h"
#endif

namespace koroutine::async_io {

std::shared_ptr<IOEngine> IOEngine::create() {
#ifdef __APPLE__
  return std::make_shared<KqueueIOEngine>();
#elif defined(__linux__)
  // return std::make_shared<EpollIOEngine>();
  throw std::runtime_error("Linux engine not implemented yet");
#elif defined(_WIN32)
  // return std::make_shared<IOCPIOEngine>();
  throw std::runtime_error("Windows engine not implemented yet");
#else
  throw std::runtime_error("Unsupported platform");
#endif
}

}  // namespace koroutine::async_io
