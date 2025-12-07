#pragma once

#ifdef __linux__
#include <liburing.h>

#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"

namespace koroutine::async_io {
class IoUringIOEngine : public IOEngine {
 public:
  IoUringIOEngine();
  ~IoUringIOEngine() override;
  void submit(std::shared_ptr<AsyncIOOp> op) override;
  void run() override;
  void stop() override;
  bool is_running() override;

 private:
  void process_op(std::shared_ptr<AsyncIOOp> op);
  void submit_sqe();

  struct io_uring ring_;
  int event_fd_;
  std::atomic<bool> running_;
  std::mutex ops_mutex_;
  std::queue<std::shared_ptr<AsyncIOOp>> pending_ops_;
  std::unordered_map<AsyncIOOp*, std::shared_ptr<AsyncIOOp>> in_flight_ops_;
};
}  // namespace koroutine::async_io
#endif  // __linux__