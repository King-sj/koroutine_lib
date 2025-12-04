#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "koroutine/koroutine.h"

using namespace koroutine;

// Demonstrates that capturing variables in a temporary lambda leads to
// use-after-free because the lambda object is destroyed before the task
// executes.
TEST(LambdaTest, CaptureLifetimeError) {
  auto task = [str = std::string("Hello World Long String To Avoid SSO")]()
      -> Task<std::string> {
    // 'str' is a member of the lambda closure.
    // Since the lambda is temporary, it is destroyed after the call to
    // operator(). Accessing 'str' here is undefined behavior (reading garbage).
    co_return str;
  }();

  // Force stack overwrite
  volatile char garbage[2048];
  for (int i = 0; i < 2048; ++i) garbage[i] = (char)i;

  std::string res = Runtime::block_on(std::move(task));

  // The value should be garbage
  EXPECT_NE(res, "Hello World Long String To Avoid SSO");
}

// Demonstrates that passing arguments by value to the coroutine lambda works
// correctly because arguments are copied to the coroutine frame.
TEST(LambdaTest, ParameterPassingFix) {
  auto task = [](std::string str) -> Task<std::string> {
    co_return str;
  }("Hello World Long String To Avoid SSO");

  // Force some stack usage
  std::vector<int> garbage(100, 0xDEADBEEF);

  std::string res = Runtime::block_on(std::move(task));

  EXPECT_EQ(res, "Hello World Long String To Avoid SSO");
}