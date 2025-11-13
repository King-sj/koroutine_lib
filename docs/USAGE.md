# Koroutine 库使用指南

## 项目简介

Koroutine 是一个基于 C++20 协程的轻量级库，提供了简单易用的协程抽象。

## 构建项目

```bash
# 配置项目
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 编译项目
cmake --build build

# 运行demo
./build/bin/koroutine_demo

# 运行测试
./build/bin/test_unit
```

## 功能特性

### 1. Task<T> - 协程任务

Task 是一个可以返回值的协程类型。

```cpp
#include "koroutine/koroutine.h"

// 返回整数的协程
Task<int> calculate() {
    co_return 42;
}

// 返回字符串的协程
Task<std::string> get_message() {
    co_return "Hello, Koroutine!";
}

// 无返回值的协程
Task<void> do_work() {
    std::cout << "Working..." << std::endl;
    co_return;
}

// 使用方式
auto task = calculate();
task.resume();  // 执行协程
int result = task.get_result();  // 获取结果
```

### 2. Generator<T> - 生成器

Generator 可以使用 `co_yield` 产生多个值。

```cpp
// 数字生成器
Generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

// 斐波那契数列生成器
Generator<int> fibonacci(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        co_yield a;
        int temp = a;
        a = b;
        b = temp + b;
    }
}

// 使用方式
auto gen = range(0, 10);
while (gen.next()) {
    std::cout << gen.value() << " ";
}
```

### debug Feature
若要启用调试输出功能，请在代码中定义 `KOROUTINE_DEBUG` 宏，如下所示：
```cpp
#define KOROUTINE_DEBUG  // Enable debug output
#include <iostream>

#include "koroutine/debug.h"
#include "koroutine/executors/async_executor.h"
#include "koroutine/executors/looper_executor.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
Task<void> loop_1() {
  while (true) {
    std::cout << "Loop 1 executing..." << std::endl;
    co_await sleep_for(500);
  }
}
Task<void> loop_2() {
  while (true) {
    std::cout << "Loop 2 executing..." << std::endl;
    co_await sleep_for(1000);
  }
}

int main() {
    // set debug level and detail flags
  debug::set_level(debug::Level::Debug);
    //   enable debug level、timestamp and thread id details
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId);
  // create a shared async executor
  std::cout << "Starting coroutine task..." << std::endl;
  auto task = []() -> Task<int> {
    //   switch to async executor
    // co_await switch_executor(std::make_shared<AsyncExecutor>());
    LOG_DEBUG("In async executor.");
    co_await sleep_for(1000);
    co_return 42;
  }();

  std::cout << "Running task and blocking for result..." << std::endl;
  int result = Runtime::block_on(std::move(task));
  std::cout << "Result: " << result << std::endl;

  //   Start two infinite loops on the same executor
  auto exec = std::make_shared<LooperExecutor>();
  Runtime::join_all(loop_1().via(exec), loop_2().via(exec));
  return 0;
}

```
## 示例程序

查看 `src/demo.cpp` 了解完整的使用示例，包括：
- Task<int> 基本使用
- Task<std::string> 字符串任务
- Task<void> 无返回值任务
- Generator 生成器
- 斐波那契数列生成器

## 测试

项目包含完整的单元测试，使用 Google Test 框架。测试文件位于 `tests/unit/test_basic.cpp`。

运行测试：
```bash
./build/bin/test_unit
```

或使用 CTest：
```bash
cd build
ctest -V
```

## API 文档

### Task<T>

| 方法 | 说明 |
|------|------|
| `void resume()` | 恢复协程执行 |
| `bool done() const` | 检查协程是否完成 |
| `T get_result()` | 获取协程返回值（会抛出异常如果协程未完成或有错误） |

### Generator<T>

| 方法 | 说明 |
|------|------|
| `bool next()` | 执行到下一个 yield 点，返回是否还有值 |
| `T value() const` | 获取当前 yield 的值 |

## 要求

- C++20 或更高版本
- 支持协程的编译器（GCC 10+, Clang 14+, MSVC 2019+）
- CMake 3.20 或更高版本

## 许可证

本项目采用 MIT 许可证。
