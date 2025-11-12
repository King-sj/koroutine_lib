#pragma once
#include "koroutine/debug.h"
#include "koroutine/executors/looper_executor.h"
#include "koroutine/schedulers/scheduler.h"
namespace koroutine {
class SimpleScheduler : public AbstractScheduler {
 public:
  SimpleScheduler() : _executor(std::make_shared<LooperExecutor>()) {}
  ~SimpleScheduler() override { _executor->shutdown(); }

  virtual void schedule(std::function<void()> func,
                        long long delay_ms = 0) override {
    // FIXME: 这里的日志没能输出出来
    LOG_TRACE("SimpleScheduler::schedule - scheduling task with delay: ",
              delay_ms);
    if (delay_ms > 0) {
      LOG_TRACE(
          "SimpleScheduler::schedule - scheduling delayed task with delay: ",
          delay_ms);
      _executor->execute_delayed(std::move(func), delay_ms);
      return;
    }
    _executor->execute(std::move(func));
  }

 private:
  std::shared_ptr<LooperExecutor> _executor;
};
}  // namespace koroutine
