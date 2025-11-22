#include <iostream>

#include "koroutine/koroutine.h"
#include "koroutine/task.hpp"

using namespace koroutine;

Task<int> simple_coro() {
  std::cout << "simple_coro: START" << std::endl;
  co_return 42;
}

Task<void> test_task() {
  std::cout << "test_task: Creating simple_coro" << std::endl;
  auto result = co_await simple_coro();
  std::cout << "test_task: Result = " << result << std::endl;
}

int main() {
  std::cout << "main: Creating task" << std::endl;
  auto task = test_task();
  std::cout << "main: Starting and waiting" << std::endl;
  Runtime::join_all(std::move(task));
  std::cout << "main: Done" << std::endl;
  return 0;
}
