#pragma once

#include <memory>

#include "executors/executor.h"

namespace koroutine {

namespace ExecutorManager {

std::shared_ptr<AbstractExecutor> get_default_executor();

void set_default_executor(std::shared_ptr<AbstractExecutor> executor);

}  // namespace ExecutorManager
}  // namespace koroutine
