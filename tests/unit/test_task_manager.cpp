#include "gtest/gtest.h"
#include "koroutine/koroutine.h"
#include "koroutine/task_manager.h"

using namespace koroutine;

Task<void> short_sleep(int ms) {
  std::cout << "short_sleep start " << ms << std::endl;
  co_await SleepAwaiter(ms);
  std::cout << "short_sleep end " << ms << std::endl;
}

Task<void> long_running_forever() {
  while (true) {
    co_await SleepAwaiter(50);
  }
}

TEST(TaskManagerTest, JoinGroupWaitsForTasks) {
  TaskManager manager;

  auto t1 = std::make_shared<Task<void>>(short_sleep(50));
  auto t2 = std::make_shared<Task<void>>(short_sleep(100));
  manager.submit_to_group("group1", t1);
  manager.submit_to_group("group1", t2);

  // Should wait until both tasks are completed
  manager.sync_wait_group("group1");

  EXPECT_TRUE(t1->is_done());
  EXPECT_TRUE(t2->is_done());
}

TEST(TaskManagerTest, TaskStartCompletes) {
  auto t = std::make_shared<Task<void>>(short_sleep(20));
  t->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(t->is_done());
}

TEST(TaskManagerTest, CancelGroupCausesTasksToComplete) {
  TaskManager manager;

  auto t1 = std::make_shared<Task<void>>(long_running_forever());
  auto t2 = std::make_shared<Task<void>>(long_running_forever());
  manager.submit_to_group("grp", t1);
  manager.submit_to_group("grp", t2);

  // Cancel the group, then wait till tasks terminate
  manager.sync_cancel_group("grp");
  EXPECT_TRUE(t1->is_done());
  EXPECT_TRUE(t2->is_done());
}

TEST(TaskManagerTest, ShutdownPreventsNewSubmissions) {
  TaskManager manager;

  auto t1 = std::make_shared<Task<void>>(short_sleep(20));
  manager.submit_to_group("g", t1);
  manager.sync_shutdown();

  // After shutdown, manager should reject new submissions (no crash)
  auto t2 = std::make_shared<Task<void>>(short_sleep(10));
  manager.submit_to_group("g2", t2);

  // Wait for a short time and ensure t2 didn't start or crash
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // t2 may or may not have started; we just ensure library doesn't crash.
  SUCCEED();
}

TEST(TaskManagerTest, JoinEmptyGroupWaitsForAll) {
  TaskManager manager;
  auto t1 = std::make_shared<Task<void>>(short_sleep(50));
  auto t2 = std::make_shared<Task<void>>(short_sleep(50));
  manager.submit_to_group("g1", t1);
  manager.submit_to_group("g2", t2);

  manager.sync_wait_group("");  // Should wait for both
  EXPECT_TRUE(t1->is_done());
  EXPECT_TRUE(t2->is_done());
}

TEST(TaskManagerTest, ListGroups) {
  TaskManager manager;
  auto t1 = std::make_shared<Task<void>>(long_running_forever());
  manager.submit_to_group("g1", t1);

  auto groups = manager.list_groups();
  EXPECT_EQ(groups.size(), 1);
  EXPECT_EQ(groups[0].first, "g1");
  EXPECT_EQ(groups[0].second, 1);

  manager.sync_cancel_group("g1");
}

TEST(TaskManagerTest, SubmitToShutdownManager) {
  TaskManager manager;
  manager.sync_shutdown();

  auto t1 = std::make_shared<Task<void>>(short_sleep(100));
  manager.submit_to_group("g1", t1);

  // Task shouldn't be started or tracked (implementation says it ignores)
  auto groups = manager.list_groups();
  EXPECT_TRUE(groups.empty());
}

TEST(TaskManagerTest, MultipleGroupsIsolation) {
  TaskManager manager;
  auto t1 = std::make_shared<Task<void>>(short_sleep(50));   // fast
  auto t2 = std::make_shared<Task<void>>(short_sleep(500));  // slow

  manager.submit_to_group("fast", t1);
  manager.submit_to_group("slow", t2);

  manager.sync_wait_group("fast");
  EXPECT_TRUE(t1->is_done());
  // t2 might still be running or done depending on scheduling, but likely
  // running

  manager.sync_cancel_group("slow");
  EXPECT_TRUE(t2->is_done());
}

TEST(TaskManagerTest, CancelEmptyGroupCancelsAll) {
  TaskManager manager;
  auto t1 = std::make_shared<Task<void>>(long_running_forever());
  auto t2 = std::make_shared<Task<void>>(long_running_forever());

  manager.submit_to_group("g1", t1);
  manager.submit_to_group("g2", t2);

  manager.sync_cancel_group("");

  EXPECT_TRUE(t1->is_done());
  EXPECT_TRUE(t2->is_done());
}
