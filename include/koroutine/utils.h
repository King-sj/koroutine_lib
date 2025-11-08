#pragma once

#include "awaiters/sleep_awaiter.hpp"

namespace koroutine {
SleepAwaiter sleep_for(long long duration_ms) {
  return SleepAwaiter(duration_ms);
}
}  // namespace koroutine