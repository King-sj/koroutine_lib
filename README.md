# Koroutine

一个轻量级的 C++20 协程库，提供简单易用的协程抽象。

## 快速开始

### 构建和运行

```bash
# 配置项目
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build

# 运行 demo
./build/bin/koroutine_demo

# 运行测试
./build/bin/test_unit
```

### 简单示例

```cpp
#include "koroutine/koroutine.h"

// Task 协程
Task<int> calculate() {
    co_return 42;
}

// Generator 生成器
Generator<int> range(int n) {
    for (int i = 0; i < n; ++i) {
        co_yield i;
    }
}

int main() {
    // 使用 Task
    auto task = calculate();
    task.resume();
    std::cout << task.get_result() << std::endl;  // 输出: 42

    // 使用 Generator
    auto gen = range(5);
    while (gen.next()) {
        std::cout << gen.value() << " ";  // 输出: 0 1 2 3 4
    }
    return 0;
}
```

## 功能特性

- ✅ **Task<T>** - 支持返回值的协程任务
- ✅ **Task<void>** - 无返回值的协程任务
- ✅ **Generator<T>** - 值生成器，支持 `co_yield`
- ✅ 异常处理
- ✅ 移动语义支持
- ✅ 零依赖（除了标准库）

## 文档

详细使用文档请查看 [USAGE.md](docs/USAGE.md)

## 要求

- C++20 或更高版本
- 支持协程的编译器
- CMake 3.20+

## 许可证

MIT License
