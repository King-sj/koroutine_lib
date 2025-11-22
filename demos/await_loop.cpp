#define KOROUTINE_DEBUG
#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace std::chrono_literals;
Task<void> completes_synchronously() { co_return; }

Task<void> loop_synchronously(int count) {
  for (int i = 0; i < count; ++i) {
    LOG_ERROR("Loop iteration ", i);
    co_await completes_synchronously();
  }
}

void recursive_function(int n) {
  if (n <= 0) {
    return;
  }
  if (n % 1000000 == 0) LOG_ERROR("Recursion level ", n);
  recursive_function(n - 1);
}

int main() {
  debug::set_level(debug::Level::Error);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  LOG_ERROR("Await Loop Demo Started");
  Runtime::block_on(loop_synchronously(1e6));
  LOG_ERROR("Await Loop Demo Finished");

  LOG_ERROR("Starting deep recursion test");
  try {
    recursive_function(1e6);
  } catch (const std::exception& e) {
    LOG_ERROR("Exception during recursion: ", e.what());
  }
  LOG_ERROR("Deep recursion test finished");
  return 0;
}
