#define KOROUTINE_DEBUG
#include "koroutine/koroutine.h"

using namespace koroutine;
Task<void> producer(Channel<int>& chan) {
  LOG_DEBUG("Producer started");
  for (int i = 0; i < 10; ++i) {
    LOG_DEBUG("Try Producing: ", i);
    co_await (chan << i);
    std::cout << "Produced: " << i << std::endl;
  }
  LOG_DEBUG("Producer finished producing items");
  //   chan.close();
  auto res = co_await chan.close_when_empty();
  if (res == -1) {
    LOG_DEBUG("Producer finished but channel not closed due to timeout");
  }
  LOG_DEBUG("Producer finished and channel closed");
}

Task<void> consumer1(Channel<int>& chan) {
  LOG_DEBUG("Consumer1 started");
  int value;
  try {
    while (true) {
      //   int value = co_await chan.read();
      co_await (chan >> value);
      std::cout << "Consumed1: " << value << std::endl;
      co_await sleep_for(100);
    }
  } catch (const Channel<int>::ChannelClosedException&) {
    std::cout << "Channel closed, consumer exiting." << std::endl;
  }
}
Task<void> consumer2(Channel<int>& chan) {
  LOG_DEBUG("Consumer2 started");
  int value;
  try {
    while (true) {
      //   int value = co_await chan.read();
      co_await (chan >> value);
      std::cout << "Consumed2: " << value << std::endl;
      co_await sleep_for(200);
    }
  } catch (const Channel<int>::ChannelClosedException&) {
    std::cout << "Channel closed, consumer exiting." << std::endl;
  }
}

int main() {
  debug::set_level(debug::Level::Trace);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  //   Runtime::block_on(afun());
  Channel<int> chan(100);
  Runtime::join_all(producer(chan), consumer1(chan), consumer2(chan));
  //   Runtime::block_on(producer(chan));
  //   Runtime::block_on(consumer1(chan));
  //   Runtime::block_on(consumer2(chan));

  return 0;
}