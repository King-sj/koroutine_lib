#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "koroutine/cancellation.hpp"
#include "koroutine/runtime.hpp"
#include "koroutine/schedulers/SimpleScheduler.h"
#include "koroutine/schedulers/schedule_request.hpp"
#include "koroutine/task.hpp"
#include "koroutine/when_all.hpp"
#include "koroutine/when_any.hpp"

using namespace koroutine;
using namespace std::chrono_literals;

// ==================== ScheduleRequest 测试 ====================

TEST(ScheduleRequestTest, BasicConstruction) {
  auto coro = []() -> Task<void> { co_return; }();
  auto handle = coro.handle_;

  ScheduleMetadata meta(ScheduleMetadata::Priority::High, "test_task");
  ScheduleRequest req(handle, meta);

  EXPECT_EQ(req.handle(), handle);
  EXPECT_EQ(req.metadata().priority, ScheduleMetadata::Priority::High);
  EXPECT_EQ(req.metadata().debug_name, "test_task");
}

TEST(ScheduleRequestTest, DefaultMetadata) {
  auto coro = []() -> Task<void> { co_return; }();
  auto handle = coro.handle_;

  ScheduleRequest req(handle);

  EXPECT_EQ(req.metadata().priority, ScheduleMetadata::Priority::Normal);
  EXPECT_TRUE(req.metadata().debug_name.empty());
}

// ==================== Scheduler 测试 ====================

TEST(SchedulerTest, ScheduleCoroutineHandle) {
  auto scheduler = std::make_shared<SimpleScheduler>();
  std::atomic<bool> resumed{false};

  auto task_lambda = [&]() -> Task<void> {
    resumed = true;
    co_return;
  };
  auto task = task_lambda();

  ScheduleRequest req(
      task.handle_,
      ScheduleMetadata(ScheduleMetadata::Priority::Normal, "test"));
  scheduler->schedule(req, 0);

  // 等待执行
  std::this_thread::sleep_for(100ms);
  EXPECT_TRUE(resumed.load());
}

TEST(SchedulerTest, ScheduleWithDelay) {
  auto scheduler = std::make_shared<SimpleScheduler>();
  auto start_time = std::chrono::steady_clock::now();
  std::atomic<bool> executed{false};

  auto task_lambda = [&]() -> Task<void> {
    executed = true;
    co_return;
  };
  auto task = task_lambda();

  scheduler->schedule(ScheduleRequest(task.handle_), 100);  // 100ms 延迟

  // 等待执行
  std::this_thread::sleep_for(200ms);
  auto elapsed = std::chrono::steady_clock::now() - start_time;

  EXPECT_TRUE(executed.load());
  EXPECT_GE(elapsed, 100ms);
}

TEST(SchedulerTest, CoAwaitSchedule) {
  auto scheduler = std::make_shared<SimpleScheduler>();
  std::chrono::milliseconds elapsed{0};
  auto task = [&]() -> Task<int> {
    auto start_time = std::chrono::steady_clock::now();
    co_await scheduler->schedule(100);  // 延迟100ms
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    co_return 42;
  };

  int result = Runtime::block_on(task());

  EXPECT_EQ(result, 42);
  EXPECT_GE(elapsed, 100ms);
}

// ==================== Continuation 测试 ====================

TEST(ContinuationTest, BasicChain) {
  auto inner = []() -> Task<int> { co_return 42; };

  auto outer = [&]() -> Task<int> {
    int value = co_await inner();
    co_return value * 2;
  };

  int result = Runtime::block_on(outer());
  EXPECT_EQ(result, 84);
}

TEST(ContinuationTest, MultiLevelChain) {
  auto level1 = []() -> Task<int> { co_return 1; };

  auto level2 = [&]() -> Task<int> {
    int v = co_await level1();
    co_return v + 10;
  };

  auto level3 = [&]() -> Task<int> {
    int v = co_await level2();
    co_return v + 100;
  };

  int result = Runtime::block_on(level3());
  EXPECT_EQ(result, 111);
}

TEST(ContinuationTest, VoidChain) {
  int counter = 0;

  auto inner = [&]() -> Task<void> {
    counter += 10;
    co_return;
  };

  auto outer = [&]() -> Task<void> {
    co_await inner();
    counter += 5;
    co_return;
  };

  Runtime::block_on(outer());
  EXPECT_EQ(counter, 15);
}

// ==================== when_all 测试 ====================

TEST(WhenAllTest, ThreeTasks) {
  std::cout << "TEST START" << std::endl;
  auto task1 = []() -> Task<int> {
    std::cout << "task1 executing" << std::endl;
    co_return 1;
  };
  auto task2 = []() -> Task<int> {
    std::cout << "task2 executing" << std::endl;
    co_return 2;
  };
  auto task3 = []() -> Task<int> {
    std::cout << "task3 executing" << std::endl;
    co_return 3;
  };

  auto combined = [&]() -> Task<std::tuple<int, int, int>> {
    std::cout << "combined start" << std::endl;
    auto result = co_await when_all(task1(), task2(), task3());
    std::cout << "combined done" << std::endl;
    co_return result;
  };

  std::cout << "before block_on" << std::endl;
  auto [r1, r2, r3] = Runtime::block_on(combined());
  std::cout << "after block_on" << std::endl;
  EXPECT_EQ(r1, 1);
  EXPECT_EQ(r2, 2);
  EXPECT_EQ(r3, 3);
}

TEST(WhenAllTest, Vector) {
  std::vector<Task<int>> tasks;
  for (int i = 0; i < 5; ++i) {
    // 使用参数传递 i，确保 i 被复制到协程帧中
    // 避免 lambda 捕获导致的悬垂引用问题（lambda 对象在协程执行前销毁）
    tasks.push_back([](int val) -> Task<int> { co_return val; }(i));
  }

  auto combined = [&]() -> Task<std::vector<int>> {
    co_return co_await when_all(std::move(tasks));
  };

  auto results = Runtime::block_on(combined());
  ASSERT_EQ(results.size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(results[i], i);
  }
}

TEST(WhenAllTest, WithDelay) {
  auto task1 = []() -> Task<int> {
    co_await std::chrono::milliseconds(50);
    co_return 1;
  };

  auto task2 = []() -> Task<int> {
    co_await std::chrono::milliseconds(100);
    co_return 2;
  };

  auto start_time = std::chrono::steady_clock::now();
  auto combined = [&]() -> Task<std::tuple<int, int>> {
    co_return co_await when_all(task1(), task2());
  };

  auto [r1, r2] = Runtime::block_on(combined());
  auto elapsed = std::chrono::steady_clock::now() - start_time;

  EXPECT_EQ(r1, 1);
  EXPECT_EQ(r2, 2);
  // 应该并发执行，总时间接近最长任务的时间
  EXPECT_GE(elapsed, 100ms);
  EXPECT_LT(elapsed, 200ms);  // 不应该是串行执行的时间
}

// ==================== when_any 测试 ====================

TEST(WhenAnyTest, FirstCompletes) {
  std::vector<Task<int>> tasks;
  tasks.push_back([]() -> Task<int> {
    co_await std::chrono::milliseconds(50);
    co_return 1;
  }());
  tasks.push_back([]() -> Task<int> {
    co_await std::chrono::milliseconds(100);
    co_return 2;
  }());
  tasks.push_back([]() -> Task<int> {
    co_await std::chrono::milliseconds(150);
    co_return 3;
  }());

  auto combined = [&]() -> Task<std::pair<size_t, int>> {
    co_return co_await when_any(std::move(tasks));
  };

  auto start_time = std::chrono::steady_clock::now();
  auto [index, result] = Runtime::block_on(combined());
  auto elapsed = std::chrono::steady_clock::now() - start_time;

  EXPECT_EQ(index, 0);  // 第一个任务最快
  EXPECT_EQ(result, 1);
  EXPECT_GE(elapsed, 50ms);
  EXPECT_LT(elapsed, 100ms);  // 不应该等待其他任务
}

// ==================== Cancellation 测试 ====================

TEST(CancellationTest, BasicCancel) {
  CancellationToken token;
  EXPECT_FALSE(token.is_cancelled());

  token.cancel();
  EXPECT_TRUE(token.is_cancelled());
}

TEST(CancellationTest, OnCancelCallback) {
  CancellationToken token;
  bool called = false;

  token.on_cancel([&]() { called = true; });

  EXPECT_FALSE(called);
  token.cancel();
  EXPECT_TRUE(called);
}

TEST(CancellationTest, OnCancelAlreadyCancelled) {
  CancellationToken token;
  token.cancel();

  bool called = false;
  token.on_cancel([&]() { called = true; });

  EXPECT_TRUE(called);  // 应该立即调用
}

TEST(CancellationTest, MultipleCancels) {
  CancellationToken token;
  int count = 0;

  token.on_cancel([&]() { count++; });
  token.on_cancel([&]() { count++; });

  token.cancel();
  EXPECT_EQ(count, 2);

  // 再次取消不应该触发回调
  token.cancel();
  EXPECT_EQ(count, 2);
}

TEST(CancellationTest, ThrowIfCancelled) {
  CancellationToken token;

  EXPECT_NO_THROW(token.throw_if_cancelled());

  token.cancel();
  EXPECT_THROW(token.throw_if_cancelled(), OperationCancelledException);
}

TEST(CancellationTest, TaskWithCancellation) {
  CancellationToken token;
  std::atomic<int> counter{0};
  std::atomic<bool> started{false};

  auto task = [&]() -> Task<void> {
    started = true;
    for (int i = 0; i < 100; ++i) {
      counter++;
      token.throw_if_cancelled();  // 检查取消
      co_await std::chrono::milliseconds(10);
    }
  };

  // 直接调用并启动
  auto t = task();
  t.with_cancellation(token).start();

  // 等待任务开始
  while (!started.load()) {
    std::this_thread::sleep_for(1ms);
  }

  // 等待一会儿后取消
  std::this_thread::sleep_for(50ms);
  token.cancel();

  // 等待一段时间让协程响应取消
  std::this_thread::sleep_for(100ms);

  // counter 应该小于 100（因为被取消了）
  EXPECT_LT(counter.load(), 100);
  EXPECT_GT(counter.load(), 0);
}

TEST(CancellationTest, CancellationSource) {
  CancellationTokenSource source;

  EXPECT_FALSE(source.is_cancelled());

  auto token = source.token();
  EXPECT_FALSE(token.is_cancelled());

  source.cancel();
  EXPECT_TRUE(source.is_cancelled());
  EXPECT_TRUE(token.is_cancelled());
}

// ==================== 综合测试 ====================

TEST(IntegrationTest, ContinuationWithWhenAll) {
  auto inner1 = []() -> Task<int> {
    co_await std::chrono::milliseconds(50);
    co_return 10;
  };

  auto inner2 = []() -> Task<int> {
    co_await std::chrono::milliseconds(30);
    co_return 20;
  };

  auto outer = [&]() -> Task<int> {
    auto [r1, r2] = co_await when_all(inner1(), inner2());
    co_return r1 + r2;
  };

  int result = Runtime::block_on(outer());
  EXPECT_EQ(result, 30);
}

TEST(IntegrationTest, SchedulerSwitching) {
  auto scheduler1 = std::make_shared<SimpleScheduler>();
  auto scheduler2 = std::make_shared<SimpleScheduler>();

  std::thread::id tid1, tid2;

  auto task = [&]() -> Task<void> {
    tid1 = std::this_thread::get_id();

    co_await scheduler2->dispatch_to();

    tid2 = std::this_thread::get_id();
    co_return;
  };

  Runtime::block_on(task());

  // 由于都是事件循环调度器，线程ID可能相同或不同
  // 这里只是验证不会崩溃
  SUCCEED();
}
