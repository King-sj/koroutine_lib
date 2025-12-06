#include <gtest/gtest.h>

#include "koroutine/koroutine.h"

using namespace koroutine;

// 测试 Runtime::block_on 能否正确获取 Task 的结果
TEST(RuntimeTest, BlockOnReturnsResult) {
  auto test_task = []() -> Task<int> { co_return 42; }();
  int result = Runtime::block_on(std::move(test_task));
  EXPECT_EQ(result, 42);
}

// 测试 Runtime::spawn 能否正确启动并分离任务
TEST(RuntimeTest, SpawnStartsAndDetachesTask) {
  std::atomic<bool> task_executed = false;
  auto test_task = [](std::atomic<bool>& task_executed) -> Task<void> {
    task_executed.store(true);
    co_return;
  }(task_executed);
  Runtime::spawn(std::move(test_task));
  // Give some time for the spawned task to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_TRUE(task_executed);
}

// 测试 Runtime::spawn 和 channel 结合使用(producer first)
TEST(RuntimeTest, SpawnWithChannelProducerFirst) {
  koroutine::Channel<int> chan(1);

  // Producer task
  auto producer = [&chan]() -> Task<void> {
    for (int i = 0; i < 5; ++i) {
      co_await chan.write(i);
    }
    co_await chan.close_when_empty();
    co_return;
  };

  // Consumer task
  auto consumer = [&chan]() -> Task<void> {
    int sum = 0;
    while (true) {
      if (!chan.is_active()) {
        break;
      }
      try {
        auto item = co_await chan.read();
        sum += item;
      } catch (const Channel<int>::ChannelClosedException&) {
        break;
      }
    }
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4);
    co_return;
  };

  Runtime::spawn(producer());
  Runtime::block_on(consumer());
}
// 测试 Runtime::spawn 和 channel 结合使用(consumer first)
TEST(RuntimeTest, SpawnWithChannelConsumerFirst) {
  koroutine::Channel<int> chan(1);
  std::atomic<bool> producer_done = false;
  // Producer task
  auto producer = [&chan, &producer_done]() -> Task<void> {
    for (int i = 0; i < 5; ++i) {
      co_await chan.write(i);
    }
    producer_done.store(true);
    co_await chan.close_when_empty();
    co_return;
  };
  // Consumer task
  auto consumer = [&chan, &producer_done]() -> Task<void> {
    int sum = 0;
    while (true) {
      if (!chan.is_active()) {
        break;
      }
      try {
        auto item = co_await chan.read();
        sum += item;
      } catch (const Channel<int>::ChannelClosedException&) {
        break;
      }
    }
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4);
    EXPECT_TRUE(producer_done.load());
    co_return;
  };
  Runtime::spawn(consumer());
  Runtime::block_on(producer());
}