#include <iostream>

#include "koroutine/koroutine.h"

using namespace koroutine;

// 异步获取用户ID
Task<int> get_user_id(const std::string& username) {
  std::cout << "Looking up user: " << username << std::endl;
  co_await std::chrono::milliseconds(100);

  if (username == "error") {
    throw std::runtime_error("User not found");
  }

  co_return 42;  // 返回用户ID
}

// 异步获取用户分数
Task<int> get_user_score(int user_id) {
  std::cout << "Getting score for user " << user_id << std::endl;
  co_await std::chrono::milliseconds(100);
  co_return user_id * 10;  // 返回分数
}

// 异步保存分数
Task<void> save_score(int score) {
  std::cout << "Saving score: " << score << std::endl;
  co_await std::chrono::milliseconds(100);
  std::cout << "Score saved successfully!" << std::endl;
}

int main() {
  std::cout << "=== 示例 1: 成功的链式调用 ===" << std::endl;

  // 创建一个协程任务包装整个链
  auto task1 = []() -> Task<void> {
    int user_id = co_await get_user_id("alice");
    std::cout << "Got user ID: " << user_id << std::endl;

    int score = co_await get_user_score(user_id);
    std::cout << "Got score: " << score << std::endl;

    co_await save_score(score);
    std::cout << "All operations completed!" << std::endl;
  }();

  auto final_task1 =
      std::move(task1)
          .catching([](std::exception& e) {
            std::cout << "Error occurred: " << e.what() << std::endl;
          })
          .finally([]() { std::cout << "Cleanup finished." << std::endl; });

  try {
    Runtime::block_on(std::move(final_task1));
  } catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << std::endl;
  }

  std::cout << "\n=== 示例 2: 带异常处理的链式调用 ===" << std::endl;

  auto task2 = get_user_id("error")
                   .catching([](std::exception& e) {
                     std::cout << "Caught error: " << e.what() << std::endl;
                   })
                   .finally([]() {
                     std::cout << "Cleanup finished (even with error)."
                               << std::endl;
                   });

  try {
    Runtime::block_on(std::move(task2));
  } catch (const std::exception& e) {
    // 异常已经在 catching 中处理，但仍会重新抛出
    std::cout << "Exception propagated: " << e.what() << std::endl;
  }

  std::cout << "\n=== 示例 3: 值转换链 ===" << std::endl;

  auto task3 =
      []() -> Task<int> {
    std::cout << "Starting with value 5" << std::endl;
    co_return 5;
  }()
                  .then([](int x) {
                    std::cout << "Multiply by 2: " << x << " -> " << x * 2
                              << std::endl;
                    return x * 2;
                  })
                  .then([](int x) {
                    std::cout << "Add 3: " << x << " -> " << x + 3 << std::endl;
                    return x + 3;
                  })
                  .then([](int x) {
                    std::cout << "Convert to string: " << x << std::endl;
                    return std::to_string(x);
                  })
                  .then([](std::string s) {
                    std::cout << "Final result: " << s << std::endl;
                  })
                  .finally([]() {
                    std::cout << "Value transformation complete!" << std::endl;
                  });

  Runtime::block_on(std::move(task3));

  return 0;
}
