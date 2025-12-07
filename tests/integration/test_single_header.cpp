#include <gtest/gtest.h>
#include <fcntl.h>

#include "koroutine/koroutine.hpp"

using namespace koroutine;

TEST(IntegrationTest, SingleHeaderBasic) {
  auto task = []() -> Task<int> { co_return 42; };

  int result = 0;
  auto wrapper = [&]() -> Task<void> { result = co_await task(); };

  Runtime::block_on(wrapper());
  EXPECT_EQ(result, 42);
}

TEST(IntegrationTest, SingleHeaderWhenAll) {
  auto task1 = []() -> Task<int> { co_return 1; };
  auto task2 = []() -> Task<int> { co_return 2; };

  auto wrapper = [&]() -> Task<void> {
    auto [r1, r2] = co_await when_all(task1(), task2());
    EXPECT_EQ(r1, 1);
    EXPECT_EQ(r2, 2);
  };

  Runtime::block_on(wrapper());
}
