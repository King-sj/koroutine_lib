#include <gtest/gtest.h>

#include "koroutine/koroutine.h"

using namespace koroutine;

TEST(ChannelTest, BasicChannel) {
  Channel<int> chan(2);  // 容量为2的通道

  auto producer = [&chan]() -> Task<void> {
    for (int i = 0; i < 5; ++i) {
      co_await (chan << i);
    }
    chan.close();
  };

  auto consumer = [&chan]() -> Task<std::vector<int>> {
    std::vector<int> received;
    try {
      while (true) {
        int value;
        co_await (chan >> value);
        received.push_back(value);
      }
    } catch (const Channel<int>::ChannelClosedException&) {
      // 通道关闭，退出循环
    }
    co_return received;
  };

  auto prod_task = producer();
  auto cons_task = consumer();

  Runtime::block_on(std::move(prod_task));
  auto received_values = Runtime::block_on(std::move(cons_task));

  std::vector<int> expected_values = {0, 1, 2, 3, 4};
  EXPECT_EQ(received_values, expected_values);
}