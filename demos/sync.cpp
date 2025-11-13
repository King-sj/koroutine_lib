// #define KOROUTINE_DEBUG
#include <queue>

#include "koroutine/debug.h"
#include "koroutine/koroutine.h"
#include "koroutine/sync/async_condition_variable.h"
#include "koroutine/sync/async_mutex.h"
using namespace koroutine;

AsyncMutex mutex;
AsyncConditionVariable cv;
std::queue<int> shared_queue;
int resume_count = 0;
bool done = false;

Task<void> producer(int id, int produce_count) {
  for (int i = 0; i < produce_count; ++i) {
    co_await mutex.lock();
    int item = id * 100 + i;
    shared_queue.push(item);
    std::cout << "Producer " << id << " produced item " << item << std::endl;
    mutex.unlock();
    cv.notify_one();
    co_await sleep_for(100);  // simulate work
  }
  //   Final producer signals completion
  co_await mutex.lock();
  resume_count--;
  if (resume_count == 0) {
    done = true;
    cv.notify_all();
  }
  mutex.unlock();
}
Task<void> consumer(int id) {
  while (true) {
    co_await mutex.lock();
    while (shared_queue.empty() && !done) {
      co_await cv.wait(mutex);
    }
    if (done && shared_queue.empty()) {
      mutex.unlock();
      break;
    }
    int item = shared_queue.front();
    shared_queue.pop();
    std::cout << "Consumer " << id << " consumed item " << item << std::endl;
    mutex.unlock();
    co_await sleep_for(150);  // simulate work
  }
}

int main() {
  debug::set_level(debug::Level::Trace);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  constexpr int producer_count = 2;
  constexpr int consumer_count = 4;
  constexpr int items_per_producer = 5;
  resume_count = producer_count;
  std::vector<Task<void>> tasks;
  // start producers
  for (int i = 0; i < producer_count; ++i) {
    tasks.push_back(producer(i + 1, items_per_producer));
  }
  // start consumers
  for (int i = 0; i < consumer_count; ++i) {
    tasks.push_back(consumer(i + 1));
  }
  Runtime::join_all(std::move(tasks));
  return 0;
}