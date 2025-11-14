#pragma once
#include <functional>

#include "koroutine/debug.h"
namespace koroutine {
class AbstractScheduler {
 public:
  virtual ~AbstractScheduler() = default;
  virtual void schedule(std::function<void()> func, long long delay_ms) = 0;
};
}  // namespace koroutine
