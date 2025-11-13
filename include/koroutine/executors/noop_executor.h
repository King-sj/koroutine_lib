#pragma once

#include "executor.h"
namespace koroutine {
class NoopExecutor : public AbstractExecutor {
 public:
  void execute(std::function<void()>&& func) override {
    LOG_TRACE("NoopExecutor::execute - executing no-op function");
    func();
    LOG_TRACE("NoopExecutor::execute - no-op function executed");
  }
};
}  // namespace koroutine