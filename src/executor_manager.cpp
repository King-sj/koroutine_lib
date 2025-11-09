#include "koroutine/executor_manager.h"

#include "koroutine/executors/looper_executor.h"
namespace koroutine {

namespace ExecutorManager {
std::shared_ptr<AbstractExecutor> get_default_executor() {
  // set looper executor as default executor
  static std::shared_ptr<AbstractExecutor> default_executor =
      std::make_shared<LooperExecutor>();
  return default_executor;
}

void set_default_executor(std::shared_ptr<AbstractExecutor> executor) {
  static std::shared_ptr<AbstractExecutor> default_executor = nullptr;
  default_executor = std::move(executor);
}
}  // namespace ExecutorManager
}  // namespace koroutine