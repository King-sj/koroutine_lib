#include <gtest/gtest.h>

#include "koroutine/koroutine.h"

using namespace koroutine;

// 测试Task协程基本功能
TEST(KoroutineTest, BasicTaskInt) {
  auto task = []() -> Task<int> { co_return 42; }();
  auto res = Runtime::block_on(std::move(task));
  EXPECT_EQ(res, 42);
}

// 测试Task<void>协程
TEST(KoroutineTest, BasicTaskVoid) {
  bool executed = false;

  auto simple_task = [&executed]() -> Task<void> {
    executed = true;
    co_return;
  };

  EXPECT_FALSE(executed);
  auto task = simple_task();
  Runtime::block_on(std::move(task));
  EXPECT_TRUE(executed);
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
  gen.for_each([&expected](int value) {
    EXPECT_EQ(value, expected);
    ++expected;
  });

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

  EXPECT_EQ(gen.next(), "Hello");

  EXPECT_EQ(gen.next(), "World");

  EXPECT_EQ(gen.next(), "Koroutine");

  EXPECT_FALSE(gen.has_next());
}

// 测试Task返回字符串
TEST(KoroutineTest, TaskWithString) {
  auto string_task = []() -> Task<std::string> {
    co_return "Hello Koroutine!";
  };

  auto task = string_task();

  EXPECT_EQ(Runtime::block_on(std::move(task)), "Hello Koroutine!");
}

// 测试多次resume的情况
TEST(KoroutineTest, MultipleResume) {
  auto simple_task = []() -> Task<int> { co_return 100; };

  auto task = simple_task();

  EXPECT_EQ(Runtime::block_on(std::move(task)), 100);
}

// 测试Generator的map、filter、flat_map等操作
TEST(KoroutineTest, GeneratorMapFilter) {
  auto expected_values = std::vector<int>{
      0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4,  5,  6,  7,  8,  9,  10, 11,
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
  };
  Generator<int>::from(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
      .filter([](auto i) { return i % 2 == 0; })
      .map([](auto i) { return i * 3; })
      .flat_map([](auto i) -> Generator<int> {
        for (int j = 0; j < i; ++j) {
          co_yield j;
        }
      })
      .take(expected_values.size())
      .for_each([=](auto i) {
        static int index = 0;
        EXPECT_EQ(i, expected_values[index++]);
      });
}
// 测试Generator的fold和sum操作
TEST(KoroutineTest, GeneratorFoldSum) {
  int sum = Generator<int>::from(1, 2, 3, 4, 5).sum();
  EXPECT_EQ(sum, 15);
  int product = Generator<int>::from(1, 2, 3, 4).fold(1, [](auto acc, auto i) {
    return acc * i;
  });
  EXPECT_EQ(product, 24);
}
// 测试Generator的 from_array 和 from_list 方法
TEST(KoroutineTest, GeneratorFromArrayAndList) {
  int array[] = {10, 20, 30};
  auto gen_from_array = Generator<int>::from_array(array, 3);
  std::vector<int> expected_array_values = {10, 20, 30};
  int index = 0;
  gen_from_array.for_each(
      [&](int value) { EXPECT_EQ(value, expected_array_values[index++]); });
  std::list<int> lst = {40, 50, 60};
  auto gen_from_list = Generator<int>::from_list(lst);
  std::vector<int> expected_list_values = {40, 50, 60};
  index = 0;
  gen_from_list.for_each(
      [&](int value) { EXPECT_EQ(value, expected_list_values[index++]); });
}
// 测试 generator fibonacci 函数
TEST(KoroutineTest, FibonacciGenerator) {
  auto fib_gen = []() -> Generator<int> {
    co_yield 0;
    co_yield 1;
    int a = 0;
    int b = 1;
    while (true) {
      co_yield a + b;
      int temp = a + b;
      a = b;
      b = temp;
    }
  };

  auto expected_values = std::vector<int>{
      0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987};

  fib_gen().take_while([](auto i) { return i < 100; }).for_each([=](auto i) {
    static int index = 0;
    EXPECT_EQ(i, expected_values[index++]);
  });
}

// 测试Task的then、catching和finally方法
TEST(KoroutineTest, TaskThenCatchingFinally) {
  bool then_called = false;
  bool catching_called = false;
  bool finally_called = false;

  auto task = []() -> Task<int> { co_return 42; }();

  task.then([&](int result) {
        then_called = true;
        EXPECT_EQ(result, 42);
      })
      .catching([&](std::exception& e) { catching_called = true; })
      .finally([&]() { finally_called = true; });
  Runtime::block_on(std::move(task));  // 确保任务完成
  EXPECT_TRUE(then_called);
  EXPECT_FALSE(catching_called);
  EXPECT_TRUE(finally_called);

  // 测试异常情况
  then_called = false;
  catching_called = false;
  finally_called = false;
  auto error_task = []() -> Task<int> {
    throw std::runtime_error("Test exception");
    co_return 0;
  }();
  error_task.then([&](int result) { then_called = true; })
      .catching([&](std::exception& e) {
        catching_called = true;
        EXPECT_STREQ(e.what(), "Test exception");
      })
      .finally([&]() { finally_called = true; });
  try {
    Runtime::block_on(std::move(error_task));  // 确保任务完成
  } catch (...) {
    // 忽略异常
  }
  EXPECT_FALSE(then_called);
  EXPECT_TRUE(catching_called);
  EXPECT_TRUE(finally_called);
}
