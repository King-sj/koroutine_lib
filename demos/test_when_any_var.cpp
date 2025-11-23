#include <iostream>
#include <string>

#include "koroutine/koroutine.h"
#include "koroutine/when_any.hpp"

using namespace koroutine;
using namespace std::chrono_literals;

Task<int> task_int() {
  co_await std::chrono::milliseconds(100);
  co_return 42;
}

Task<std::string> task_string() {
  co_await std::chrono::milliseconds(50);
  co_return "hello";
}

Task<void> async_main() {
  auto result = co_await when_any(task_int(), task_string());

  if (result) {
    std::visit(
        [](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, int>) {
            std::cout << "Got int: " << arg << std::endl;
          } else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "Got string: " << arg << std::endl;
          }
        },
        *result);
  } else {
    std::cout << "Error occurred" << std::endl;
  }

  co_return;
}

int main() {
  debug::set_level(debug::Level::Trace);
  Runtime::block_on(async_main());
  return 0;
}
