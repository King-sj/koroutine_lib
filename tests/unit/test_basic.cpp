#include <gtest/gtest.h>

#include "koroutine/koroutine.h"

using namespace koroutine;

// 测试Task协程基本功能
TEST(KoroutineTest, BasicTaskInt) {
  auto simple_task = []() -> Task<int> { co_return 42; };

  auto task = simple_task();
  task.resume();

  EXPECT_TRUE(task.done());
  EXPECT_EQ(task.get_result(), 42);
}

// 测试Task<void>协程
TEST(KoroutineTest, BasicTaskVoid) {
  bool executed = false;

  auto simple_task = [&executed]() -> Task<void> {
    executed = true;
    co_return;
  };

  auto task = simple_task();
  EXPECT_FALSE(executed);

  task.resume();

  EXPECT_TRUE(executed);
  EXPECT_TRUE(task.done());
}

// 测试Generator生成器
TEST(KoroutineTest, BasicGenerator) {
  auto number_generator = []() -> Generator<int> {
    for (int i = 0; i < 5; ++i) {
      co_yield i;
    }
  };

  auto gen = number_generator();

  int expected = 0;
  while (gen.next()) {
    EXPECT_EQ(gen.value(), expected);
    ++expected;
  }

  EXPECT_EQ(expected, 5);
}

// 测试字符串生成器
TEST(KoroutineTest, StringGenerator) {
  auto string_generator = []() -> Generator<std::string> {
    co_yield "Hello";
    co_yield "World";
    co_yield "Koroutine";
  };

  auto gen = string_generator();

  EXPECT_TRUE(gen.next());
  EXPECT_EQ(gen.value(), "Hello");

  EXPECT_TRUE(gen.next());
  EXPECT_EQ(gen.value(), "World");

  EXPECT_TRUE(gen.next());
  EXPECT_EQ(gen.value(), "Koroutine");

  EXPECT_FALSE(gen.next());
}

// 测试Task返回字符串
TEST(KoroutineTest, TaskWithString) {
  auto string_task = []() -> Task<std::string> {
    co_return "Hello Koroutine!";
  };

  auto task = string_task();
  task.resume();

  EXPECT_TRUE(task.done());
  EXPECT_EQ(task.get_result(), "Hello Koroutine!");
}

// 测试多次resume的情况
TEST(KoroutineTest, MultipleResume) {
  auto simple_task = []() -> Task<int> { co_return 100; };

  auto task = simple_task();

  task.resume();
  EXPECT_TRUE(task.done());

  // 多次调用resume不应该崩溃
  task.resume();
  task.resume();

  EXPECT_EQ(task.get_result(), 100);
}
