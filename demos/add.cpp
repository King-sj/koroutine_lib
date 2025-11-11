#define KOROUTINE_DEBUG
#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace std::chrono_literals;
Task<int> simple_task2() {
  // sleep 1 秒
  LOG_DEBUG("simple_task2 - started");
  co_await 1s;
  co_return 2;
}

Task<int> simple_task3() {
  // sleep 2 秒
  LOG_DEBUG("simple_task3 - started");
  co_await 2s;
  co_return 3;
}

Task<int> simple_task() {
  LOG_DEBUG("simple_task - started");
  // result2 == 2
  auto result2 = co_await simple_task2();
  LOG_DEBUG("simple_task - simple_task2 completed with result: ", result2);
  // result3 == 3
  auto result3 = co_await simple_task3();
  LOG_DEBUG("simple_task - completed with results: ", result2, ", ", result3);
  co_return 1 + result2 + result3;
}

int main() {
  LOG_DEBUG("Add Demo Started");
  std::cout << "Starting simple_task..." << std::endl;
  debug::set_level(debug::Level::Trace);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  LOG_DEBUG("creating simple_task");
  auto task = simple_task();
  task.then([](int result) {
        std::cout << "Task completed with result: " << result << std::endl;
      })
      .catching([](std::exception& e) {
        std::cout << "Task failed with exception: " << e.what() << std::endl;
      })
      .finally(
          []() { std::cout << "Task has finished execution." << std::endl; });
  std::shared_ptr<AbstractExecutor> executor =
      std::make_shared<NewThreadExecutor>();
  LOG_DEBUG("Submitting task to NewThreadExecutor");
  Runtime::block_on(std::move(task.via(executor)));
  LOG_DEBUG("Add Demo Finished");
  return 0;
}
