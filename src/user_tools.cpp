#include "koroutine/user_tools.h"
namespace koroutine {
SleepAwaiter sleep_for(long long duration_ms) {
  return SleepAwaiter(duration_ms);
}

SwitchExecutorAwaiter switch_executor(std::shared_ptr<AbstractExecutor> ex) {
  // FIXME: co_await switch_executor(...) ，只有这一个 awaiter
  // 成功切换了执行器，其他 awaiter 都没有成功切换执行器
  return SwitchExecutorAwaiter(std::move(ex));
}
}  // namespace koroutine