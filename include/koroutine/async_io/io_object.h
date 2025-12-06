#pragma once
#include "koroutine/task.hpp"
namespace koroutine::async_io {
// enum class IOObjectType { File, Socket, Pipe, Timer, Signal, Other };
enum class OpType {
  READ,
  WRITE,
  CLOSE,
  CONNECT,
  ACCEPT,
  OPEN,
  RECVFROM,
  SENDTO,
};
// 前向声明
class IOEngine;

class AsyncIOOp;

template <typename T>
struct IOAwaiter;

class AsyncIOObject {
 public:
  virtual ~AsyncIOObject() = default;
  // 基础IO操作
  virtual Task<size_t> read(void* buf, size_t size) = 0;
  virtual Task<size_t> write(const void* buf, size_t size) = 0;
  virtual Task<void> close() = 0;
  // 获取底层句柄(平台相关)
  virtual intptr_t native_handle() const = 0;

  std::shared_ptr<IOEngine> get_engine() const { return engine_; }

 protected:
  friend class IOEngine;

  template <typename T>
  friend struct IOAwaiter;

  explicit AsyncIOObject(std::shared_ptr<IOEngine> engine)
      : engine_(std::move(engine)) {}
  std::shared_ptr<IOEngine> engine_;  // 关联的IO引擎
};

}  // namespace koroutine::async_io