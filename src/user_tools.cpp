#include "koroutine/user_tools.h"
namespace koroutine {
SleepAwaiter sleep_for(long long duration_ms) {
  return SleepAwaiter(duration_ms);
}

SwitchExecutorAwaiter switch_executor(std::shared_ptr<AbstractExecutor> ex) {
  return SwitchExecutorAwaiter(std::move(ex));
}
}  // namespace koroutine