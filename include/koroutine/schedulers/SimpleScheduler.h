#pragma once
#include "koroutine/debug.h"
#include "koroutine/executors/looper_executor.h"
#include "koroutine/schedulers/scheduler.h"
namespace koroutine {
class SimpleScheduler : public AbstractScheduler {
 public:
  SimpleScheduler() : _executor(std::make_shared<LooperExecutor>()) {}
  ~SimpleScheduler() override { _executor->shutdown(); }

  // 引入基类的 schedule(long long) 方法
  using AbstractScheduler::dispatch_to;
  using AbstractScheduler::schedule;

  // 实现核心接口：调度 ScheduleRequest
  void schedule(ScheduleRequest request, long long delay_ms = 0) override {
    LOG_TRACE("SimpleScheduler::schedule - scheduling request with delay: ",
              delay_ms);

    if (!request) {
      LOG_ERROR("SimpleScheduler::schedule - invalid request (null handle)");
      return;
    }

    LOG_DEBUG("SimpleScheduler::schedule - request debug name: ",
              request.metadata().debug_name);

    if (delay_ms > 0) {
      LOG_TRACE(
          "SimpleScheduler::schedule - scheduling delayed request with delay: ",
          delay_ms);
      _executor->execute_delayed(
          [req = std::move(request)]() mutable { req.resume(); }, delay_ms);
    } else {
      _executor->execute(
          [req = std::move(request)]() mutable { req.resume(); });
    }
  }

 private:
  std::shared_ptr<LooperExecutor> _executor;
};
}  // namespace koroutine
