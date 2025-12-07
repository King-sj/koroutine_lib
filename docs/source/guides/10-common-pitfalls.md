# 注意点

## Lambda 捕获与生命周期陷阱

在使用 Lambda 表达式创建协程（返回 `Task<T>`）时，必须格外小心变量的生命周期。

### ❌ 错误做法：在临时 Lambda 中捕获变量

即使是**按值捕获**（Capture by Value），如果 Lambda 表达式本身是一个临时对象，且在协程开始执行前就被销毁，那么协程体内访问捕获的变量也会导致 **Use-After-Free**（悬垂引用）。

这是因为 C++ 协程会将 Lambda 的捕获变量存储在 Lambda 对象本身中，而不是自动复制到协程帧（Coroutine Frame）里。当 `task` 被创建时，Lambda 临时对象可能已经析构。

```cpp
// 错误示例：捕获的变量随 Lambda 销毁而失效
auto task = [str = std::string("Hello")]() -> Task<std::string> {
  // 危险！str 是 Lambda 对象的成员。
  // 如果 Lambda 是临时的，它会在 operator() 返回后立即销毁。
  // 当协程稍后被调度执行时，str 所在的内存已经是垃圾值。
  co_return str;
}(); // Lambda 在这里被调用并销毁
```

### ✅ 正确做法：通过参数传递

将需要的数据作为**参数**传递给 Lambda。C++ 协程机制会将参数复制（或移动）到协程帧中，从而保证其生命周期与协程一致。

```cpp
// 正确示例：参数会被复制到协程帧中
auto task = [](std::string str) -> Task<std::string> {
  // 安全。str 是协程参数，存在于协程帧中。
  co_return str;
}("Hello");
```

### 循环中的应用

在循环创建任务时，这一点尤为重要：

```cpp
#define KOROUTINE_DEBUG
#include <vector>

#include "koroutine/executors/new_thread_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace std::chrono_literals;

Task<void> async_main() {
  std::vector<Task<int>> tasks;
  for (int i = 0; i < 3; ++i) {
    // ✅ 正确：使用参数传递 i，确保 i 被复制到协程帧中
    // ❌ 错误：如果写成 [i](){...}()，i 将作为 Lambda 成员，随 Lambda 销毁而失效
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
```