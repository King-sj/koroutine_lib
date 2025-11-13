// #include <gtest/gtest.h>

// #include <mutex>
// #include <thread>
// #include <vector>

// #include "koroutine/koroutine.h"
// using namespace koroutine;

// TEST(ExecutorTest, SharedAsyncConcurrency) {
//   auto exec = std::make_shared<koroutine::AsyncExecutor>();

//   auto t1 = []() -> Task<int> {
//     using namespace std::chrono_literals;
//     co_await koroutine::SleepAwaiter(std::chrono::milliseconds(200));
//     co_return 1;
//   }();

//   auto t2 = []() -> Task<int> {
//     using namespace std::chrono_literals;
//     co_await koroutine::SleepAwaiter(std::chrono::milliseconds(200));
//     co_return 2;
//   }();

//   t1.via(exec);
//   t2.via(exec);

//   auto start = std::chrono::steady_clock::now();
//   Runtime::join_all(std::move(t1), std::move(t2));
//   auto elapsed = std::chrono::steady_clock::now() - start;

//   EXPECT_LT(
//       std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
//       400);
// }

// TEST(ExecutorTest, LooperExecutorDelayedExecution) {
//   auto looper = std::make_shared<LooperExecutor>();

//   auto task = [](std::thread::id looper_thread_id) -> Task<std::thread::id> {
//     using namespace std::chrono_literals;
//     co_await koroutine::SleepAwaiter(100ms);
//     co_return std::this_thread::get_id();
//   }(looper->get_thread_id());

//   task.via(looper);

//   auto start = std::chrono::steady_clock::now();
//   auto result_thread_id = task.get_result();
//   auto elapsed = std::chrono::steady_clock::now() - start;

//   EXPECT_EQ(result_thread_id, looper->get_thread_id());
//   EXPECT_GE(
//       std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
//       100);
// }

// TEST(ExecutorTest, SwitchExecutor) {
//   auto async_exec = std::make_shared<AsyncExecutor>();
//   auto looper_exec = std::make_shared<LooperExecutor>();

//   auto task = [&]() -> Task<bool> {
//     EXPECT_NE(std::this_thread::get_id(), looper_exec->get_thread_id());

//     co_await switch_executor(looper_exec);

//     EXPECT_EQ(std::this_thread::get_id(), looper_exec->get_thread_id());

//     co_await switch_executor(async_exec);

//     EXPECT_NE(std::this_thread::get_id(), looper_exec->get_thread_id());

//     co_return true;
//   }();

//   task.via(async_exec);
//   EXPECT_TRUE(task.get_result());
// }

// // 启动两个长循环在同一个执行器上，测试其是否能交替执行
// TEST(ExecutorTest, LooperExecutorConcurrentLoops) {
//   auto looper = std::make_shared<LooperExecutor>();
//   std::vector<int> results;
//   const int cnt = 5;
//   auto loop_1 = [&]() -> Task<void> {
//     for (int i = 0; i < cnt; ++i) {
//       results.push_back(1);
//       co_await sleep_for(50);
//     }
//   }();
//   auto loop_2 = [&]() -> Task<void> {
//     for (int i = 0; i < cnt; ++i) {
//       results.push_back(2);
//       co_await sleep_for(100);
//     }
//   }();

//   loop_1.via(looper);
//   loop_2.via(looper);
//   Runtime::join_all(std::move(loop_1), std::move(loop_2));
//   // 检查同一个执行器上的两个循环是否交替执行，
//   // 即结果数组中不应有太长的连续相同数字
//   int last = -1;
//   int consecutive_count = 0;
//   const int max_consecutive_allowed = 3;  // 允许的最大连续相同数字数量
//   for (int value : results) {
//     if (value == last) {
//       consecutive_count++;
//       EXPECT_LE(consecutive_count, max_consecutive_allowed);
//     } else {
//       last = value;
//       consecutive_count = 1;
//     }
//   }
// }