#include "koroutine/task.hpp"

namespace koroutine {

Task<void> TaskPromise<void>::get_return_object() {
  return Task<void>{
      std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}
TaskAwaiter<void> TaskPromise<void>::await_transform(Task<void>&& task) {
  return await_transform(TaskAwaiter<void>(std::move(task)));
}

}  // namespace koroutine
