#pragma once
#include <functional>
namespace koroutine {
class DelayedExecutable {
 public:
  DelayedExecutable(std::function<void()>&& func, long long delay)
      : func(std::move(func)) {
    using namespace std;
    using namespace std::chrono;
    auto now = system_clock::now();
    auto current = duration_cast<milliseconds>(now.time_since_epoch()).count();

    scheduled_time = current + delay;
  }
  long long delay() const {
    using namespace std;
    using namespace std::chrono;

    auto now = system_clock::now();
    auto current = duration_cast<milliseconds>(now.time_since_epoch()).count();
    return scheduled_time - current;
  }
  long long get_scheduled_time() const { return scheduled_time; }
  void operator()() { func(); }

 private:
  long long scheduled_time;
  std::function<void()> func;
};

class DelayedExecutableCompare {
 public:
  bool operator()(DelayedExecutable& left, DelayedExecutable& right) {
    return left.get_scheduled_time() > right.get_scheduled_time();
  }
};

}  // namespace koroutine