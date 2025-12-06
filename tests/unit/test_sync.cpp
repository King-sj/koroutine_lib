#include <gtest/gtest.h>

#include <memory>

#include "koroutine/koroutine.h"
#include "koroutine/sync/async_condition_variable.h"
#include "koroutine/sync/async_mutex.h"
using namespace koroutine;

TEST(SyncTest, SyncTest_BasicMutex_Test) {
  AsyncMutex mtx;
  EXPECT_TRUE(mtx.try_lock());
  EXPECT_FALSE(mtx.try_lock());
  mtx.unlock();
  EXPECT_FALSE(mtx.try_lock() == false);
  mtx.unlock();
}

TEST(SyncTest, SyncTest_BasicConditionVariable_Test) {
  AsyncMutex mtx;
  AsyncConditionVariable cv;

  bool notified = false;

  auto waiter_lambda = [&]() -> Task<void> {
    co_await mtx.lock();
    while (!notified) {
      co_await cv.wait(mtx);
    }
    mtx.unlock();
  };
  auto waiter = waiter_lambda();

  auto notifier_lambda = [&]() -> Task<void> {
    co_await sleep_for(100);  // ensure waiter is waiting
    co_await mtx.lock();
    notified = true;
    cv.notify_one();
    mtx.unlock();
  };
  auto notifier = notifier_lambda();

  Runtime::join_all(std::move(waiter), std::move(notifier));

  EXPECT_TRUE(notified);
}

TEST(SyncTest, AsyncMutexLocking) {
  AsyncMutex mtx;
  int counter = 0;
  const int increments = 200;

  auto t1_lambda = [&]() -> Task<void> {
    for (int i = 0; i < increments; ++i) {
      co_await mtx.lock();
      int tmp = counter;
      co_await sleep_for(1);
      counter = tmp + 1;
      mtx.unlock();
    }
  };
  auto t1 = t1_lambda();

  auto t2_lambda = [&]() -> Task<void> {
    for (int i = 0; i < increments; ++i) {
      co_await mtx.lock();
      int tmp = counter;
      co_await sleep_for(1);
      counter = tmp + 1;
      mtx.unlock();
    }
  };
  auto t2 = t2_lambda();

  Runtime::join_all(std::move(t1), std::move(t2));

  EXPECT_EQ(counter, increments * 2);
}

TEST(SyncTest, ConditionVariableNotifyOne) {
  AsyncMutex mtx;
  AsyncConditionVariable cv;
  int shared = 0;

  auto waiter_lambda = [&]() -> Task<void> {
    co_await mtx.lock();
    while (shared == 0) {
      co_await cv.wait(mtx);
    }
    // at this point, waiter should own the mutex
    shared += 1;
    mtx.unlock();
  };
  auto waiter = waiter_lambda();

  auto notifier_lambda = [&]() -> Task<void> {
    // give waiter a chance to start and wait
    co_await sleep_for(10);
    co_await mtx.lock();
    shared = 1;
    cv.notify_one();
    mtx.unlock();
  };
  auto notifier = notifier_lambda();

  Runtime::join_all(std::move(waiter), std::move(notifier));

  EXPECT_EQ(
      shared,
      2);  // waiter increments once, notifier set to 1 then waiter adds 1 -> 2
}
