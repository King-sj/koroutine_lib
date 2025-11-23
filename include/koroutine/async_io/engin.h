#pragma once
#include <memory>
#include <thread>

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

  static std::shared_ptr<IOEngine> create();  // 工厂方法：创建平台相关引擎
 protected:
  // 完成IO操作后唤醒协程
  void complete(std::shared_ptr<AsyncIOOp> op) { op->complete(); }
};

// 获取默认的 IO 引擎 (单例)
inline std::shared_ptr<IOEngine> get_default_io_engine() {
  static auto engine = [] {
    auto eng = IOEngine::create();
    // 在后台线程运行默认引擎
    std::thread([eng] { eng->run(); }).detach();
    return eng;
  }();
  return engine;
}

}  // namespace koroutine::async_io