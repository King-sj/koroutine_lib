#pragma once

#if defined(__APPLE__)

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"

namespace koroutine::async_io {
class KqueueIOEngine : public IOEngine {
 public:
  KqueueIOEngine();
  ~KqueueIOEngine() override;
  void submit(std::shared_ptr<AsyncIOOp> op) override;
  void run() override;
  void stop() override;
  bool is_running() override;

 private:
  void process_read(std::shared_ptr<AsyncIOOp> op);
  void process_write(std::shared_ptr<AsyncIOOp> op);
  void process_close(std::shared_ptr<AsyncIOOp> op);

  int kqueue_fd_;  // kqueue 文件描述符
  std::atomic<bool> running_;
  std::mutex ops_mutex_;
  std::queue<std::shared_ptr<AsyncIOOp>> pending_ops_;  // 待处理的操作队列

  // 用于跟踪正在监听的文件描述符和对应的操作
  std::unordered_map<intptr_t, std::shared_ptr<AsyncIOOp>> active_ops_;
};
}  // namespace koroutine::async_io

#endif  // __APPLE__
