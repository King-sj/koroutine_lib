# Task 与 Generator

`Task` 和 `Generator` 是 `koroutine_lib` 提供的两种核心协程类型。`Task` 用于表示一个最终会完成的异步操作，而 `Generator` 用于创建一个可以按需生成值序列的迭代器。

## 1. `Task<T>`: 异步任务

`Task<T>` 是库中最常用的组件。它代表一个可能正在进行中的计算，这个计算最终会产生一个 `T` 类型的结果，或者 `void`（如果 `T` 是 `void`），或者抛出异常。

### 基本用法

一个返回 `Task<T>` 的函数就是一个协程。你可以使用 `co_return` 来返回值。

```cpp
#include "koroutine/koroutine.h"
#include <iostream>

// 一个简单的 Task，异步返回一个整数
Task<int> get_answer() {
    co_await sleep_for(std::chrono::seconds(1)); // 模拟耗时工作
    co_return 42;
}

// 一个无返回值的 Task
Task<void> print_message() {
    co_await sleep_for(std::chrono::milliseconds(500));
    std::cout << "Hello from a Task<void>!" << std::endl;
}

int main() {
    // 使用 Runtime::block_on 来同步等待并获取 Task 的结果
    int answer = Runtime::block_on(get_answer());
    std::cout << "The answer is: " << answer << std::endl;

    // 对于 Task<void>，Runtime::block_on 会等待它执行完毕
    Runtime::block_on(print_message());

    return 0;
}
```

### 链式调用与组合

`Task` 的强大之处在于其可组合性。你可以在一个协程中 `co_await` 另一个 `Task`，形成清晰的、线性的异步逻辑流。

```cpp
// 异步获取用户ID
Task<int> get_user_id(const std::string& username) {
  std::cout << "Looking up user: " << username << std::endl;
  co_await sleep_for(std::chrono::milliseconds(100));
  co_return 42;
}

// 异步获取用户分数
Task<int> get_user_score(int user_id) {
  std::cout << "Getting score for user " << user_id << std::endl;
  co_await sleep_for(std::chrono::milliseconds(100));
  co_return user_id * 10;
}

// 将多个异步操作串联起来
Task<void> process_user(const std::string& username) {
    int user_id = co_await get_user_id(username);
    std::cout << "Got user ID: " << user_id << std::endl;

    int score = co_await get_user_score(user_id);
    std::cout << "Got score: " << score << std::endl;
}

int main() {
    Runtime::block_on(process_user("alice"));
}
```

这个例子中，`process_user` 看起来就像普通的同步代码，但实际上每个 `co_await` 都可能涉及一次挂起和恢复，完全不会阻塞主线程。

### 异常处理: `.catching()`

如果一个 `Task` 内部抛出异常，这个异常会被捕获并可以在调用链中处理。使用 `.catching()` 方法可以附加一个异常处理器。

```cpp
Task<int> might_throw() {
    co_await sleep_for(std::chrono::milliseconds(100));
    throw std::runtime_error("Something went wrong!");
    co_return 0; // 不会执行到这里
}

int main() {
    auto task = might_throw();

    // 使用 .catching() 来处理异常
    auto handled_task = std::move(task).catching([](std::exception& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
    });

    // Runtime::block_on 会执行任务，如果发生异常，处理器会被调用
    Runtime::block_on(std::move(handled_task));
}
```

### 清理操作: `.finally()`

`.finally()` 方法允许你附加一个无论 `Task` 成功还是失败都会执行的清理回调。这对于释放资源非常有用。

```cpp
Task<void> work_with_resource() {
    std::cout << "Acquiring resource..." << std::endl;
    co_await sleep_for(std::chrono::milliseconds(200));
    std::cout << "Work done." << std::endl;
}

int main() {
    auto task = work_with_resource();

    auto final_task = std::move(task).finally([]() {
        std::cout << "Releasing resource..." << std::endl;
    });

    Runtime::block_on(std::move(final_task));
}
```

## 2. `Generator<T>`: 值序列生成器

`Generator<T>` 是一种特殊的协程，它不返回单个值，而是使用 `co_yield` 产生一个值的序列。它就像一个可以暂停和恢复的函数，每次恢复时都产生下一个值。

### 基本用法

```cpp
#include "koroutine/generator.hpp"
#include <iostream>

// 一个生成从 start 到 end-1 的整数序列的 Generator
Generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        std::cout << "(Yielding " << i << ")" << std::endl;
        co_yield i;
    }
}

int main() {
    auto numbers = range(0, 5);

    // 使用 has_next() 和 next() 遍历
    while (numbers.has_next()) {
        std::cout << "Got value: " << numbers.next() << std::endl;
    }
}
```

输出：

```
(Yielding 0)
Got value: 0
(Yielding 1)
Got value: 1
(Yielding 2)
Got value: 2
(Yielding 3)
Got value: 3
(Yielding 4)
Got value: 4
```

注意，`range` 函数中的代码和 `main` 函数中的循环是交错执行的。

### 链式操作

`Generator` 也支持函数式风格的链式操作，如 `map`, `take` 等。

```cpp
Generator<int> numbers = range(0, 10);

// 创建一个新的 Generator，它只取前5个偶数并平方
auto result_generator = std::move(numbers)
    .take_while([](int i) { return i < 10; }) // 实际上这里可以省略，因为range只到10
    .filter([](int i) { return i % 2 == 0; }) // 假设有 filter
    .take(5)
    .map([](int i) { return i * i; });

while (result_generator.has_next()) {
    std::cout << result_generator.next() << " "; // 输出: 0 4 16 36 64
}
```
