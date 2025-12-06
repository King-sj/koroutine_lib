<div style="display: flex; gap: 10px;">
  <a href="https://img.shields.io/badge/C++-23-blue.svg">
    <img src="https://img.shields.io/badge/C++-23-blue.svg" alt="C++23">
  </a>
    <a href="https://img.shields.io/github/actions/workflow/status/King-sj/koroutine_lib/cmake.yml?branch=main">
      <img src="https://img.shields.io/github/actions/workflow/status/King-sj/koroutine_lib/cmake.yml?branch=main" alt="Build Status">
    </a>
  <a href="https://opensource.org/licenses/MPL-2.0">
    <img src="https://img.shields.io/badge/License-MPL-brightgreen.svg" alt="License: MPL">
  </a>
</div>

# Koroutine

一个基于 C++23 的现代化协程库，专为结构化并发和高性能异步 I/O 设计。

`koroutine_lib` 提供了一套完整、易于使用的工具集，帮助开发者轻松驾驭复杂的异步编程挑战。从简单的并发任务到跨平台的网络 I/O，它都提供了类型安全、高效率的解决方案。

## ✨ 核心特性

- 🚀 **现代 C++ 设计**: 完全基于 C++23 协程，拥抱最新标准。
- ⚡ **强大的执行模型**:
  - **Executors**: 内置多种执行器，如 `NewThreadExecutor`、`LooperExecutor` (事件循环) 和 `AsyncExecutor` (线程池)。
  - **Schedulers**: 抽象调度层，可实现自定义任务调度逻辑（如优先级、时间轮）。
- 🔗 **结构化并发**:
  - 使用 `when_all` 和 `when_any` 轻松组合和并发执行多个任务。
  - 作用域内的任务自动等待，防止资源泄漏。
- 🤝 **安全的协程间通信**:
  - 提供 `Channel<T>`，可在不同协程（甚至不同线程）之间安全地传递数据。
- 📁 **跨平台异步 I/O**:
  - 统一的 `async_io` 接口，支持文件和套接字操作。
  - 底层自动选择最高效的 I/O 模型 (`io_uring` on Linux, `kqueue` on macOS, `IOCP` on Windows)。
- 🔍 **易于调试**:
  - 内置轻量级日志系统，可追踪协程生命周期和调度细节。
- ✅ **零依赖**: 除了 C++ 标准库，不依赖任何第三方库。

## 🧭 Task Manager

`TaskManager` 是一个用于管理一组协程任务（以组为单位）的轻量级工具。它支持：

- 提交任务到分组并自动启动
- 取消分组内的所有任务（通过 `CancellationToken`）
- 等待分组内所有任务完成（同步和异步）

示例：

```cpp
koroutine::TaskManager manager;

auto t = std::make_shared<koroutine::Task>([]() -> koroutine::Task<void> {
  co_await koroutine::SleepAwaiter(1000);
});

manager.submit_to_group("group1", t);
// 等待完成
manager.sync_wait_group("group1");
```

更多详情请参考 `include/koroutine/task_manager.h`。

## 快速开始

### 示例：并发下载任务

下面的例子展示了如何使用 `koroutine_lib` 并发执行两个模拟的下载任务，并等待它们全部完成。

```cpp
#include "koroutine/koroutine.h"
#include "koroutine/executors/async_executor.h"
#include <iostream>

// 模拟一个耗时的下载任务
Task<std::string> download_file(const std::string& url) {
    std::cout << "开始下载: " << url << " on thread " << std::this_thread::get_id() << std::endl;

    // 模拟耗时操作
    co_await sleep_for(std::chrono::seconds(2));

    std::cout << "下载完成: " << url << std::endl;
    co_return "Content of " + url;
}

// 主协程任务
Task<void> main_task() {
    std::cout << "启动主任务 on thread " << std::this_thread::get_id() << std::endl;

    // 并发执行两个下载任务
    auto [result1, result2] = co_await when_all(
        download_file("https://example.com/file1.zip"),
        download_file("https://example.com/file2.zip")
    );

    std::cout << "所有任务完成!" << std::endl;
    std::cout << "结果 1: " << result1 << std::endl;
    std::cout << "结果 2: " << result2 << std::endl;
}

int main() {

    // 在主调度器上运行主任务
    Runtime::block_on(main_task());

    return 0;
}
```

### 构建和运行

```bash
# 1. 配置项目 (需要 C++23 编译器)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. 编译
cmake --build build

# 3. 运行 demo (以 task_chain_demo 为例)
./build/bin/task_chain_demo

# 4. 运行所有测试
cd build && ctest -V
```

## 架构概览

`koroutine_lib` 的核心由以下几个部分组成：

- **`Task<T>`**: 协程任务的表示。它是一个惰性、可等待的 future-like 对象。
- **`Executor`**: 任务的执行上下文，决定了协程的恢复在哪个线程上执行。
- **`Scheduler`**: 调度器，管理一个或多个 `Executor`，并根据策略分发任务。
- **`Awaiter`**: `co_await` 的操作对象，封装了协程的挂起和恢复逻辑。
- **`async_io`**: 异步 I/O 操作的抽象层。

这些组件协同工作，提供了一个强大而灵活的异步编程框架。

## 文档

更详细的设计文档、API 参考和使用指南，请访问我们的 **[在线文档](https://docs.koroutine.bupt.online/)** 。

你也可以在 `docs/` 目录下找到原始 Markdown 文档。

## 要求

- C++23 或更高版本
- 支持协程的编译器 (GCC 11+, Clang 14+, MSVC 19.29+)
- CMake 3.20+

## 许可证

本项目采用 [MPL License](LICENSE)。

## 致谢

- HTTP 模块基于 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 修改而来，感谢原作者 yhirose 的出色工作。
