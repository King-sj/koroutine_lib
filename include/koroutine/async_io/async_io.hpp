#pragma once
#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/file.h"
#include "koroutine/async_io/mock.h"
#include "koroutine/async_io/socket.h"
#include "koroutine/async_io/standard_stream.h"

// 根据平台包含相应的IO引擎实现
#ifdef __linux__
#include "koroutine/async_io/platform/linux/io_uring_engin.h"
#elif defined(__APPLE__)
#include "koroutine/async_io/platform/macos/kqueue_engin.h"
#elif defined(_WIN64)
#include "koroutine/async_io/platform/win/iocp_engin.h"
#endif

namespace koroutine::async_io {

// 打开文件(工厂函数)
inline Task<std::shared_ptr<AsyncIOObject>> async_open(
    const std::string& path, std::ios::openmode mode) {
  auto engine = get_default_io_engine();
  auto file = co_await AsyncFile::open(engine, path, mode);
  co_return file;
}

// 网络连接(工厂函数)
inline Task<std::shared_ptr<AsyncSocket>> async_connect(const std::string& host,
                                                        uint16_t port) {
  auto engine = get_default_io_engine();
  auto socket = co_await AsyncSocket::connect(engine, host, port);
  co_return socket;
}

// 创建Mock异步IO对象(工厂函数)
inline Task<std::shared_ptr<MockAsyncIOObject>> create_mock_io_object() {
  auto engine = get_default_io_engine();
  co_return std::make_shared<MockAsyncIOObject>(engine);
}

// 实现 static std::shared_ptr<IOEngine> IOEngine::create()
inline std::shared_ptr<IOEngine> IOEngine::create() {
#ifdef __linux__
  return std::make_shared<IoUringIOEngine>();
#elif defined(__APPLE__)
  return std::make_shared<KqueueIOEngine>();
#elif defined(_WIN64)
  return std::make_shared<IOCPIOEngine>();
#else
  static_assert(false, "Unsupported platform for IOEngine");
#endif
}

}  // namespace koroutine::async_io