#pragma once

#if defined(__APPLE__)

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
};
}  // namespace koroutine::async_io

#endif  // __APPLE__
