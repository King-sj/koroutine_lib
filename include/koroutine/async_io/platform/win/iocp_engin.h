#pragma once

#if defined(_WIN64)
#include <mutex>
#include <unordered_map>

#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"
namespace koroutine::async_io {
class IOCPIOEngine : public IOEngine {
 public:
  IOCPIOEngine();
  ~IOCPIOEngine() override;
  void submit(std::shared_ptr<AsyncIOOp> op) override;
  void run() override;
  void stop() override;
  bool is_running() override;

 private:
  void* iocp_handle_;
  std::atomic<bool> running_;
  std::mutex ops_mutex_;
  std::unordered_map<void*, std::shared_ptr<AsyncIOOp>> in_flight_ops_;
};
}  // namespace koroutine::async_io
#endif  // _WIN64