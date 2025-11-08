#pragma once

#include "executor.h"
namespace koroutine {
class NoopExecutor : public AbstractExecutor {
 public:
  void execute(std::function<void()>&& func) override { func(); }
};
}  // namespace koroutine