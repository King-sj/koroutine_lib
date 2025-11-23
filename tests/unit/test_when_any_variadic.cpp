#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "koroutine/koroutine.h"
#include "koroutine/when_any.hpp"

using namespace koroutine;
using namespace std::chrono_literals;

TEST(WhenAnyVariadicTest, MixedTypes) {
  auto task1 = []() -> Task<int> {
    co_await std::chrono::milliseconds(100);
    co_return 42;
  };

  auto task2 = []() -> Task<std::string> {
    co_await std::chrono::milliseconds(50);
    co_return "hello";
  };

  auto combined = [&]() -> Task<void> {
    auto result = co_await when_any(task1(), task2());
    EXPECT_TRUE(result.has_value());

    bool visited = false;
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, std::string>) {
            EXPECT_EQ(arg, "hello");
            visited = true;
          } else {
            ADD_FAILURE() << "Expected string result";
            return;
          }
        },
        *result);
    EXPECT_TRUE(visited);
  };

  Runtime::block_on(combined());
}

TEST(WhenAnyVariadicTest, FirstCompletes) {
  auto task1 = []() -> Task<int> {
    co_await std::chrono::milliseconds(200);
    co_return 1;
  };

  auto task2 = []() -> Task<int> {
    co_await std::chrono::milliseconds(50);
    co_return 2;
  };

  auto combined = [&]() -> Task<void> {
    auto result = co_await when_any(task1(), task2());
    EXPECT_TRUE(result.has_value());

    std::visit([&](auto&& arg) { EXPECT_EQ(arg, 2); }, *result);
  };

  auto start = std::chrono::steady_clock::now();
  Runtime::block_on(combined());
  auto end = std::chrono::steady_clock::now();

  EXPECT_LT(end - start, 150ms);
}

TEST(WhenAnyVariadicTest, ExceptionHandling) {
  auto task1 = []() -> Task<int> {
    co_await std::chrono::milliseconds(50);
    throw std::runtime_error("oops");
    co_return 1;
  };

  auto task2 = []() -> Task<int> {
    co_await std::chrono::milliseconds(200);
    co_return 2;
  };

  auto combined = [&]() -> Task<void> {
    auto result = co_await when_any(task1(), task2());
    EXPECT_FALSE(result.has_value());

    try {
      std::rethrow_exception(result.error());
    } catch (const std::runtime_error& e) {
      EXPECT_STREQ(e.what(), "oops");
    } catch (...) {
      ADD_FAILURE() << "Expected runtime_error";
    }
  };

  Runtime::block_on(combined());
}
