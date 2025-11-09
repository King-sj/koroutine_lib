#include "koroutine/task.hpp"

namespace koroutine {

Task<void> TaskPromise<void>::get_return_object() {
  return Task<void>{
      std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}

}  // namespace koroutine
