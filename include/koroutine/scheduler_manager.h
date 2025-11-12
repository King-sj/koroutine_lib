#pragma once

#include <memory>

#include "schedulers/scheduler.h"

namespace koroutine {

namespace SchedulerManager {

std::shared_ptr<AbstractScheduler> get_default_scheduler();

void set_default_scheduler(std::shared_ptr<AbstractScheduler> scheduler);

}  // namespace SchedulerManager
}  // namespace koroutine
