#pragma once
#include <memory>

#include "koroutine/async_io/op.h"

namespace koroutine::async_io {
class AsyncIOOp;  // 前向声明

class IOEngine {
 public:
  virtual ~IOEngine() = default;
  // 提交异步IO操作
  virtual void submit(std::shared_ptr<AsyncIOOp> op) = 0;

  virtual void run() = 0;         // 运行IO引擎的事件循环
  virtual void stop() = 0;        // 停止IO引擎的事件循环
  virtual bool is_running() = 0;  // 检查IO引擎是否在运行

  static std::unique_ptr<IOEngine> create();  // 工厂方法：创建平台相关引擎
 protected:
  // 完成IO操作后唤醒协程
  void complete(std::shared_ptr<AsyncIOOp> op) {
    op->coro_handle.resume();  // 恢复协程
  }
};
}  // namespace koroutine::async_io