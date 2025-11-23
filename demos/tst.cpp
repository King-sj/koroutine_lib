#define KOROUTINE_DEBUG
#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace std::chrono_literals;

Task<int> t1() {
  LOG_DEBUG("t1 executing");
  co_return 1;
}
Task<int> t2() {
  LOG_DEBUG("t2 executing");
  co_return 2;
}
Task<int> t3() {
  LOG_DEBUG("t3 executing");
  co_return 3;
}

Task<void> async_main() {
  auto [r1, r2, r3] = co_await when_all(t1(), t2(), t3());
  LOG_DEBUG("async_main - all tasks completed");
  LOG_INFO("Results: ", r1, ", ", r2, ", ", r3);
  co_return;
}

int main() {
  debug::set_level(debug::Level::Debug);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  Runtime::block_on(async_main());
  return 0;
}
