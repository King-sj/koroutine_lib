#define KOROUTINE_DEBUG
#include <vector>

#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace std::chrono_literals;

Task<void> async_main() {
  std::vector<Task<int>> tasks;
  for (int i = 0; i < 3; ++i) {
    // 使用参数传递 i，确保 i 被复制到协程帧中
    // 避免 lambda 捕获导致的悬垂引用问题（lambda 对象在协程执行前销毁）
    tasks.push_back([](int val) -> Task<int> { co_return val; }(i));
  }
  auto results = co_await when_all(std::move(tasks));
  for (int i = 0; i < 3; ++i) {
    std::cout << "Result " << i << ": " << results[i] << std::endl;
  }

  co_return;
}

int main() {
  debug::set_level(debug::Level::Trace);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  Runtime::block_on(async_main());
  return 0;
}
