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

  // 使用右值引用调用链式方法
  auto final_task =
      []() -> Task<int> { co_return 42; }()
                  .then([&](int result) {
                    then_called = true;
                    EXPECT_EQ(result, 42);
                    return result;  // 传递结果
                  })
                  .catching([&](std::exception&) { catching_called = true; })
                  .finally([&]() { finally_called = true; });

  Runtime::block_on(std::move(final_task));  // 确保任务完成
  EXPECT_TRUE(then_called);
  EXPECT_FALSE(catching_called);
  EXPECT_TRUE(finally_called);

  // 测试异常情况
  then_called = false;
  catching_called = false;
  finally_called = false;
  auto error_final_task =
      []() -> Task<int> {
    throw std::runtime_error("Test exception");
    co_return 0;
  }()
                  .then([&](int) {
                    then_called = true;
                    return 0;
                  })
                  .catching([&](std::exception& e) {
                    catching_called = true;
                    EXPECT_STREQ(e.what(), "Test exception");
                  })
                  .finally([&]() { finally_called = true; });

  try {
    Runtime::block_on(std::move(error_final_task));  // 确保任务完成
  } catch (...) {
    // 忽略异常
  }
  EXPECT_FALSE(then_called);
  EXPECT_TRUE(catching_called);
  EXPECT_TRUE(finally_called);
}

// 测试 then 方法的值转换
TEST(TaskChainTest, ThenValueTransformation) {
  auto task = []() -> Task<int> { co_return 5; }()
                          .then([](int x) {
                            EXPECT_EQ(x, 5);
                            return x * 2;
                          })
                          .then([](int x) {
                            EXPECT_EQ(x, 10);
                            return x + 3;
                          })
                          .then([](int x) {
                            EXPECT_EQ(x, 13);
                            return std::to_string(x);
                          });

  auto result = Runtime::block_on(std::move(task));
  EXPECT_EQ(result, "13");
}

// 测试 then 方法从值类型到 void
TEST(TaskChainTest, ThenValueToVoid) {
  bool lambda1_called = false;
  bool lambda2_called = false;

  auto task = []() -> Task<int> { co_return 42; }()
                          .then([&](int x) {
                            lambda1_called = true;
                            EXPECT_EQ(x, 42);
                          })
                          .then([&]() { lambda2_called = true; });

  Runtime::block_on(std::move(task));
  EXPECT_TRUE(lambda1_called);
  EXPECT_TRUE(lambda2_called);
}

// 测试 then 方法从 void 到值类型
TEST(TaskChainTest, ThenVoidToValue) {
  auto task = []()
      -> Task<void> { co_return; }().then([]() { return 100; }).then([](int x) {
    EXPECT_EQ(x, 100);
    return x * 2;
  });

  auto result = Runtime::block_on(std::move(task));
  EXPECT_EQ(result, 200);
}

// 测试 then 方法链式传递 void
TEST(TaskChainTest, ThenVoidChain) {
  int counter = 0;

  auto task = []() -> Task<void> { co_return; }()
                          .then([&]() { counter++; })
                          .then([&]() { counter++; })
                          .then([&]() { counter++; });

  Runtime::block_on(std::move(task));
  EXPECT_EQ(counter, 3);
}

// 测试 catching 方法捕获异常
TEST(TaskChainTest, CatchingHandlesException) {
  bool catching_called = false;
  std::string error_message;

  auto task = []() -> Task<int> {
    throw std::runtime_error("Test error");
    co_return 0;
  }()
                          .catching([&](std::exception& e) {
                            catching_called = true;
                            error_message = e.what();
                          });

  try {
    Runtime::block_on(std::move(task));
    FAIL() << "Should have thrown exception";
  } catch (const std::exception& e) {
    EXPECT_TRUE(catching_called);
    EXPECT_EQ(error_message, "Test error");
    EXPECT_STREQ(e.what(), "Test error");
  }
}

// 测试 catching 不会捕获成功的任务
TEST(TaskChainTest, CatchingDoesNotTriggerOnSuccess) {
  bool catching_called = false;

  auto task = []() -> Task<int> { co_return 42; }().catching(
                       [&](std::exception&) { catching_called = true; });

  auto result = Runtime::block_on(std::move(task));
  EXPECT_EQ(result, 42);
  EXPECT_FALSE(catching_called);
}

// 测试 finally 在成功时执行
TEST(TaskChainTest, FinallyExecutesOnSuccess) {
  bool finally_called = false;

  auto task = []() -> Task<int> { co_return 123; }().finally(
                       [&]() { finally_called = true; });

  auto result = Runtime::block_on(std::move(task));
  EXPECT_EQ(result, 123);
  EXPECT_TRUE(finally_called);
}

// 测试 finally 在异常时也执行
TEST(TaskChainTest, FinallyExecutesOnException) {
  bool finally_called = false;

  auto task = []() -> Task<int> {
    throw std::runtime_error("Error");
    co_return 0;
  }()
                          .finally([&]() { finally_called = true; });

  try {
    Runtime::block_on(std::move(task));
    FAIL() << "Should have thrown exception";
  } catch (const std::exception&) {
    EXPECT_TRUE(finally_called);
  }
}

// 测试 then + catching + finally 组合（成功路径）
TEST(TaskChainTest, FullChainSuccess) {
  bool then_called = false;
  bool catching_called = false;
  bool finally_called = false;

  auto task =
      []() -> Task<int> { co_return 10; }()
                  .then([&](int x) {
                    then_called = true;
                    return x * 2;
                  })
                  .catching([&](std::exception&) { catching_called = true; })
                  .finally([&]() { finally_called = true; });

  auto result = Runtime::block_on(std::move(task));
  EXPECT_EQ(result, 20);
  EXPECT_TRUE(then_called);
  EXPECT_FALSE(catching_called);
  EXPECT_TRUE(finally_called);
}

// 测试 then + catching + finally 组合（异常路径）
TEST(TaskChainTest, FullChainException) {
  bool then_called = false;
  bool catching_called = false;
  bool finally_called = false;

  auto task =
      []() -> Task<int> {
    throw std::runtime_error("Error");
    co_return 0;
  }()
                  .then([&](int x) {
                    then_called = true;
                    return x;
                  })
                  .catching([&](std::exception&) { catching_called = true; })
                  .finally([&]() { finally_called = true; });

  try {
    Runtime::block_on(std::move(task));
    FAIL() << "Should have thrown exception";
  } catch (const std::exception&) {
    EXPECT_FALSE(then_called);
    EXPECT_TRUE(catching_called);
    EXPECT_TRUE(finally_called);
  }
}

// 测试多个 then 链式调用
TEST(TaskChainTest, MultipleThenChain) {
  auto task = []() -> Task<int> { co_return 1; }()
                          .then([](int x) { return x + 1; })
                          .then([](int x) { return x * 2; })
                          .then([](int x) { return x + 10; })
                          .then([](int x) { return x * 3; });

  auto result = Runtime::block_on(std::move(task));
  // (((1 + 1) * 2) + 10) * 3 = (2 * 2 + 10) * 3 = 14 * 3 = 42
  EXPECT_EQ(result, 42);
}

// 测试 void Task 的 catching
TEST(TaskChainTest, VoidTaskCatching) {
  bool catching_called = false;

  auto task = []() -> Task<void> {
    throw std::runtime_error("Void error");
    co_return;
  }()
                          .catching([&](std::exception& e) {
                            catching_called = true;
                            EXPECT_STREQ(e.what(), "Void error");
                          });

  try {
    Runtime::block_on(std::move(task));
    FAIL() << "Should have thrown exception";
  } catch (const std::exception&) {
    EXPECT_TRUE(catching_called);
  }
}

// 测试 void Task 的 finally
TEST(TaskChainTest, VoidTaskFinally) {
  bool finally_called = false;

  auto task = []() -> Task<void> { co_return; }().finally(
                       [&]() { finally_called = true; });

  Runtime::block_on(std::move(task));
  EXPECT_TRUE(finally_called);
}

// 测试异常在 then 中抛出
TEST(TaskChainTest, ExceptionInThen) {
  bool catching_called = false;

  auto task = []() -> Task<int> { co_return 42; }()
                          .then([](int) -> int {
                            throw std::runtime_error("Error in then");
                            return 0;
                          })
                          .catching([&](std::exception& e) {
                            catching_called = true;
                            EXPECT_STREQ(e.what(), "Error in then");
                          });

  try {
    Runtime::block_on(std::move(task));
    FAIL() << "Should have thrown exception";
  } catch (const std::exception&) {
    EXPECT_TRUE(catching_called);
  }
}

// 测试多个 catching 调用（只有第一个应该执行）
TEST(TaskChainTest, MultipleCatching) {
  int catching_count = 0;

  auto task = []() -> Task<int> {
    throw std::runtime_error("Error");
    co_return 0;
  }()
                          .catching([&](std::exception&) { catching_count++; })
                          .catching([&](std::exception&) { catching_count++; });

  try {
    Runtime::block_on(std::move(task));
  } catch (...) {
  }

  // 两个 catching 都应该被调用，因为异常在第一个 catching 后被重新抛出
  EXPECT_EQ(catching_count, 2);
}

// 测试多个 finally 调用
TEST(TaskChainTest, MultipleFinally) {
  int finally_count = 0;

  auto task = []() -> Task<int> { co_return 42; }()
                          .finally([&]() { finally_count++; })
                          .finally([&]() { finally_count++; })
                          .finally([&]() { finally_count++; });

  Runtime::block_on(std::move(task));
  EXPECT_EQ(finally_count, 3);
}
