#pragma once
#include <coroutine>

#include "koroutine/async_io/io_object.h"
namespace koroutine::async_io {
class AsyncIOOp {
 public:
  OpType type;                               // 操作类型
  std::shared_ptr<AsyncIOObject> io_object;  // 关联的IO对象
  void* buffer;                              // 数据缓冲区
  size_t size;                               // 请求的大小
  size_t actual_size;                        // 实际处理的大小
  std::error_code error;                     // 操作结果（错误码）
  std::coroutine_handle<> coro_handle;       // 协程句柄

  AsyncIOOp(OpType op_type, std::shared_ptr<AsyncIOObject> obj, void* buf,
            size_t sz)
      : type(op_type),
        io_object(obj),
        buffer(buf),
        size(sz),
        actual_size(0),
        error() {}
  // 回调函数：由IO引擎调用
  void operator()() {
    if (!error) {
      // 操作成功，恢复协程
      coro_handle.resume();
    } else {
      // TODO: 错误处理(可抛异常或设置结果)
      coro_handle.resume();
    }
  }
};
}  // namespace koroutine::async_io