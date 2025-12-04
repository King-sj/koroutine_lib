#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

namespace koroutine::async_io {

// 基础 Socket 选项模板
template <int Level, int Name, typename T = int>
class SocketOption {
 public:
  SocketOption() = default;
  explicit SocketOption(T value) : value_(value) {}

  // 获取选项值
  T value() const { return value_; }

  // 设置选项值
  void set_value(T value) { value_ = value; }

  // 隐式转换为值类型
  operator T() const { return value_; }

  // 用于 setsockopt/getsockopt 的底层接口
  int level(int /*protocol*/) const { return Level; }
  int name(int /*protocol*/) const { return Name; }
  const void* data(int /*protocol*/) const { return &value_; }
  void* data(int /*protocol*/) { return &value_; }
  socklen_t size(int /*protocol*/) const { return sizeof(value_); }
  void resize(int /*protocol*/, socklen_t /*new_size*/) {
    // 对于固定大小类型，通常不需要 resize，但在某些变长选项中可能需要
  }

 private:
  T value_{0};
};

// 布尔类型的选项通常在底层也是用 int 表示
template <int Level, int Name>
class BooleanSocketOption : public SocketOption<Level, Name, int> {
 public:
  BooleanSocketOption() = default;
  explicit BooleanSocketOption(bool value)
      : SocketOption<Level, Name, int>(value ? 1 : 0) {}

  bool value() const { return SocketOption<Level, Name, int>::value() != 0; }
  void set_value(bool value) {
    SocketOption<Level, Name, int>::set_value(value ? 1 : 0);
  }
  operator bool() const { return value(); }
};

// --- 常用选项定义 ---

// TCP_NODELAY: 禁用 Nagle 算法
using TcpNoDelay = BooleanSocketOption<IPPROTO_TCP, TCP_NODELAY>;

// SO_REUSEADDR: 允许重用本地地址
using ReuseAddress = BooleanSocketOption<SOL_SOCKET, SO_REUSEADDR>;

// SO_KEEPALIVE: 保持连接检测
using KeepAlive = BooleanSocketOption<SOL_SOCKET, SO_KEEPALIVE>;

// SO_RCVBUF: 接收缓冲区大小
using ReceiveBufferSize = SocketOption<SOL_SOCKET, SO_RCVBUF, int>;

// SO_SNDBUF: 发送缓冲区大小
using SendBufferSize = SocketOption<SOL_SOCKET, SO_SNDBUF, int>;

// SO_DEBUG: 调试模式
using DebugMode = BooleanSocketOption<SOL_SOCKET, SO_DEBUG>;

#ifdef SO_REUSEPORT
// SO_REUSEPORT: 允许多个 Socket 绑定同一端口 (Linux/macOS)
using ReusePort = BooleanSocketOption<SOL_SOCKET, SO_REUSEPORT>;
#endif

}  // namespace koroutine::async_io
