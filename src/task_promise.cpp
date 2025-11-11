#include "koroutine/task.hpp"

namespace koroutine {

Task<void> TaskPromise<void>::get_return_object() {
  LOG_TRACE("TaskPromise<void>::get_return_object - creating Task<void>");
  return Task<void>{
      std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}
TaskAwaiter<void> TaskPromise<void>::await_transform(Task<void>&& task) {
  LOG_TRACE("TaskPromise<void>::await_transform - transforming Task<void>");
  return await_transform(TaskAwaiter<void>(std::move(task)));
}

}  // namespace koroutine
