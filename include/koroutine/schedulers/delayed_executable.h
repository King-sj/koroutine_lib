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
  // priority_queue 需要可调用 const T& 的比较器，这里改为 const 引用并声明为
  // const 成员。
  bool operator()(const DelayedExecutable& left,
                  const DelayedExecutable& right) const {
    return left.get_scheduled_time() > right.get_scheduled_time();
  }
};

}  // namespace koroutine