#include "koroutine/scheduler_manager.h"

#include "koroutine/schedulers/SimpleScheduler.h"
namespace koroutine {

namespace SchedulerManager {
static std::shared_ptr<AbstractScheduler> default_scheduler =
    std::make_shared<SimpleScheduler>();
std::shared_ptr<AbstractScheduler> get_default_scheduler() {
  // set looper executor as default executor
  return default_scheduler;
}

void set_default_scheduler(std::shared_ptr<AbstractScheduler> scheduler) {
  default_scheduler = std::move(scheduler);
}
}  // namespace SchedulerManager
}  // namespace koroutine