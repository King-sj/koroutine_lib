#pragma once

#include "awaiters/sleep_awaiter.hpp"
#include "awaiters/switch_executor.hpp"

namespace koroutine {
SleepAwaiter sleep_for(long long duration_ms);

SwitchExecutorAwaiter switch_executor(std::shared_ptr<AbstractExecutor> ex);

}  // namespace koroutine