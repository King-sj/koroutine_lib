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
};
// 前向声明
class IOEngine;

class AsyncIOOp;

class AsyncIOObject {
 public:
  virtual ~AsyncIOObject() = default;
  // 基础IO操作
  virtual Task<size_t> read(void* buf, size_t size) = 0;
  virtual Task<size_t> write(const void* buf, size_t size) = 0;
  virtual Task<void> close() = 0;
  // 获取底层句柄(平台相关)
  virtual intptr_t native_handle() const = 0;

 protected:
  friend class IOEngine;
  explicit AsyncIOObject(IOEngine& engine) : engine_(engine) {}
  IOEngine& engine_;  // 关联的IO引擎
};

}  // namespace koroutine::async_io