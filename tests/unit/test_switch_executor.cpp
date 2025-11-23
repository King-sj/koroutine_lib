#include <gtest/gtest.h>

#include <thread>

#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"

using namespace koroutine;

TEST(SwitchExecutorTest, SwitchThreads) {
  auto executor = std::make_shared<NewThreadExecutor>();

  auto task = [executor]() -> Task<void> {
    auto thread_id_1 = std::this_thread::get_id();

    // Switch to a new thread
    co_await switch_to(executor);

    auto thread_id_2 = std::this_thread::get_id();
    EXPECT_NE(thread_id_1, thread_id_2);

    // Switch back to another new thread (NewThreadExecutor spawns a new thread
    // for each execute)
    co_await switch_to(executor);
    auto thread_id_3 = std::this_thread::get_id();
    EXPECT_NE(thread_id_2, thread_id_3);
    EXPECT_NE(thread_id_1, thread_id_3);
  };

  Runtime::block_on(task());
}
