#pragma once

#include <memory>

#include "../coroutine_common.h"
#include "../executors/executor.h"
#include "awaiter.hpp"
namespace koroutine {

struct SwitchExecutorAwaiter : public AwaiterBase<void> {
  explicit SwitchExecutorAwaiter(std::shared_ptr<AbstractExecutor> to) noexcept
      : target(std::move(to)) {}

 protected:
  void after_suspend() override {
    // switch to target executor
    this->install_executor(target);
    this->resume();
  }

 private:
  std::shared_ptr<AbstractExecutor> target;
};

}  // namespace koroutine
