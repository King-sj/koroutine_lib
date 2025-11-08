#pragma once
#include <functional>

namespace koroutine {

class AbstractExecutor {
 public:
  virtual void execute(std::function<void()>&& func) = 0;
};

}  // namespace koroutine