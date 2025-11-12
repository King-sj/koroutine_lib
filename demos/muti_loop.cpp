// #define KOROUTINE_DEBUG
#include <iostream>

#include "koroutine/debug.h"
#include "koroutine/executors/async_executor.h"
#include "koroutine/executors/looper_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
int sum1, sum2, sum3, sum4;
const int delta = 0;
Task<void> loop_1() {
  while (true) {
    // std::cout << "Loop 1 executing..." << std::endl;
    co_await sleep_for(500 + delta);
    sum1++;
    // std::cout << "Sum1: " << sum << std::endl;
  }
}
Task<void> loop_2() {
  while (true) {
    // std::cout << "Loop 2 executing..." << std::endl;
    co_await sleep_for(1000 + delta);
    sum2++;
    // std::cout << "Sum2: " << sum << std::endl;
  }
}
Task<void> loop_3() {
  while (true) {
    // std::cout << "Loop 3 executing..." << std::endl;
    co_await sleep_for(1500 + delta);
    sum3++;
    // std::cout << "Sum3: " << sum << std::endl;
  }
}
Task<void> loop_4() {
  while (true) {
    // std::cout << "Loop 4 executing..." << std::endl;
    co_await sleep_for(3000 + delta);
    sum4++;
    // std::cout << "Sum4: " << sum << std::endl;
  }
}
Task<void> watch() {
  while (true) {
    co_await sleep_for(2000);
    // std::cout << "Status: sum1=" << sum1 << ", sum2=" << sum2
    //           << ", sum3=" << sum3 << ", sum4=" << sum4 << std::endl;
    double rate1 = 1.0 * sum1 / sum1;
    double rate2 = 1.0 * sum1 / sum2;
    double rate3 = 1.0 * sum1 / sum3;
    double rate4 = 1.0 * sum1 / sum4;
    std::cout << "Rates: loop1=" << rate1 << ", loop2=" << rate2
              << ", loop3=" << rate3 << ", loop4=" << rate4 << std::endl;
  }
}

int main() {
  debug::set_level(debug::Level::Debug);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId);
  // create a shared async executor
  std::cout << "Starting coroutine task..." << std::endl;
  auto task = []() -> Task<int> {
    std::cout << "Task started, sleeping for 2 seconds..." << std::endl;
    co_await sleep_for(2000);
    std::cout << "Woke up, returning result 42." << std::endl;
    co_return 42;
  }();

  std::cout << "Running task and blocking for result..." << std::endl;
  int result = Runtime::block_on(std::move(task));
  std::cout << "Result: " << result << std::endl;

  //   Start two infinite loops on the same executor
  auto exec = std::make_shared<LooperExecutor>();
  Runtime::join_all(loop_4(), loop_2(), loop_1(), loop_3(), watch());
  return 0;
}
