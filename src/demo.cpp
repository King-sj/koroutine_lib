#define KOROUTINE_DEBUG
#include <iostream>

#include "koroutine/debug.h"
#include "koroutine/executors/async_executor.h"
#include "koroutine/executors/looper_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
Task<void> loop_1() {
  while (true) {
    std::cout << "Loop 1 executing..." << std::endl;
    co_await sleep_for(500);
  }
}
Task<void> loop_2() {
  while (true) {
    std::cout << "Loop 2 executing..." << std::endl;
    co_await sleep_for(1000);
  }
}

int main() {
  debug::set_level(debug::Level::Debug);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId);
  // create a shared async executor
  std::cout << "Starting coroutine task..." << std::endl;
  auto task = []() -> Task<int> {
    //   switch to async executor
    // co_await switch_executor(std::make_shared<AsyncExecutor>());
    LOG_DEBUG("In async executor.");
    co_await sleep_for(1000);
    co_return 42;
  }();

  std::cout << "Running task and blocking for result..." << std::endl;
  int result = Runtime::block_on(std::move(task));
  std::cout << "Result: " << result << std::endl;

  //   Start two infinite loops on the same executor
  auto exec = std::make_shared<LooperExecutor>();
  Runtime::join_all(loop_1().via(exec), loop_2().via(exec));
  return 0;
}
