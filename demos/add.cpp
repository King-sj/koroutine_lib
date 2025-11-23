#define KOROUTINE_DEBUG
#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace std::chrono_literals;
Task<int> simple_task2() {
  // sleep 1 秒
  LOG_DEBUG("simple_task2 - started");
  std::cout << "simple_task2: Sleeping for 1 second..." << std::endl;
  co_await sleep_for(1000);
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
  debug::set_level(debug::Level::Debug);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  LOG_DEBUG("Add Demo Started");

  LOG_DEBUG("creating simple_task");
  auto final_task =
      simple_task()
          .then([](int result) {
            std::cout << "Task completed with result: " << result << std::endl;
            return result;
          })
          .catching([](std::exception& e) {
            std::cout << "Task failed with exception: " << e.what()
                      << std::endl;
          })
          .finally([]() {
            std::cout << "Task has finished execution." << std::endl;
          });
  try {
    Runtime::block_on(std::move(final_task));
  } catch (const std::exception& e) {
    std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
  }
  LOG_DEBUG("Add Demo Finished");
  return 0;
}
