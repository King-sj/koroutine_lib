# 执行器与调度器

在 `koroutine_lib` 中，**执行器 (Executor)** 和 **调度器 (Scheduler)** 是控制协程在何处、何时运行的核心组件。理解它们是掌握高级异步编程的关键。

## 1. `Executor`: 在哪里执行？

`Executor` 是一个非常简单的概念：它是一个对象，可以接受一个函数（`std::function<void()>`）并执行它。它代表了“执行上下文”，即你的代码实际运行的地方。

`koroutine_lib` 提供了几种内置的 `Executor`：

- **`NewThreadExecutor`**: 最简单粗暴的执行器。每次调用 `execute`，它都会创建一个全新的 `std::thread` 来运行任务，然后立即分离 (detach)。
  - **优点**: 简单，任务之间完全隔离。
  - **缺点**: 创建线程的开销很大，不适合大量、短小的任务。

- **`LooperExecutor`**: 该执行器内部维护一个独立的事件循环线程。所有提交给它的任务都会被放入一个队列中，由该线程按顺序执行。
  - **优点**: 保证任务在同一个线程上串行执行，非常适合需要线程亲和性的场景（如 UI 更新、访问非线程安全资源）。
  - **缺点**: 如果一个任务阻塞，会阻塞后续所有任务。

- **`AsyncExecutor`**: 一个基于 `std::async` 的线程池执行器。它会将任务提交给 C++ 标准库的默认线程池去执行。
  - **优点**: 重用线程，开销比 `NewThreadExecutor` 小，适合通用的计算密集型任务。
  - **缺点**: 无法精细控制线程数量和行为。

## 2. `Scheduler`: 如何调度？

如果说 `Executor` 是“工人”，那么 `Scheduler` 就是“工头”。`Scheduler` 管理一个或多个 `Executor`，并根据特定的策略决定将任务分派给哪个“工人”。

`Scheduler` 是一个更高级的抽象，它允许你实现复杂的调度逻辑，例如：

- **负载均衡**: 将任务分发给最空闲的 `Executor`。
- **优先级调度**: 优先执行高优先级的任务。
- **时间调度**: 在指定时间点或延迟后执行任务。

### `SchedulerManager`

为了方便管理，`koroutine_lib` 提供了 `SchedulerManager`。这是一个全局服务，用于创建和访问预设的、覆盖大多数应用场景的调度器：

- **计算调度器 (`Compute Scheduler`)**: 通常由一个 `AsyncExecutor` 或其他线程池支持，专为 CPU 密集型任务设计。
- **I/O 调度器 (`IO Scheduler`)**: 可以是另一个独立的线程池，用于处理可能阻塞的网络或文件 I/O 操作。

```cpp
// 在程序启动时创建调度器
auto compute_scheduler = SchedulerManager::create_compute_scheduler(4); // 4个线程
auto io_scheduler = SchedulerManager::create_io_scheduler();
```

## 3. `co_await switch_to(scheduler)`: 在协程中切换上下文

`koroutine_lib` 最强大的功能之一，就是允许协程在不同的调度器（即不同的线程或线程池）之间无缝切换。这是通过 `co_await switch_to(scheduler)` 实现的。

`switch_to` 会返回一个 `Awaiter`。当 `co_await` 这个 `Awaiter` 时：
1.  当前协程会挂起。
2.  `Awaiter` 将协程的恢复句柄（continuation）提交给你指定的 `Scheduler`。
3.  该 `Scheduler` 会在它管理的 `Executor` 上安排恢复操作。
4.  当协程恢复时，它已经运行在新的线程上下文中了。

### 示例：将 I/O 操作卸载到 I/O 线程

假设我们有一个任务，它需要先在主线程上做一些计算，然后执行一个耗时的文件写入，最后再回到主线程更新状态。

```cpp
Task<void> process_data_and_write_to_file(const std::string& data) {
    // 当前在默认调度器上 (例如，主线程或计算线程池)
    std::cout << "1. 准备数据 on thread " << std::this_thread::get_id() << std::endl;
    auto processed_data = data + " [processed]";

    // 切换到 I/O 调度器来执行文件操作
    co_await switch_to(SchedulerManager::get_io_scheduler());

    std::cout << "2. 写入文件 on thread " << std::this_thread::get_id() << std::endl;
    // 模拟耗时的文件写入
    co_await sleep_for(std::chrono::seconds(2));
    // std::ofstream file("output.txt"); file << processed_data;

    // (可选) 切换回默认调度器
    co_await switch_to(SchedulerManager::get_default_scheduler());

    std::cout << "3. 操作完成 on thread " << std::this_thread::get_id() << std::endl;
    co_return;
}

int main() {
    // 设置调度器
    SchedulerManager::create_compute_scheduler(1); // 默认调度器
    SchedulerManager::create_io_scheduler();       // IO调度器

    Runtime::block_on(process_data_and_write_to_file("my_data"));
}
```

**输出可能会是这样：**
```
1. 准备数据 on thread 0x10e9c7000
2. 写入文件 on thread 0x16f5b3000  // <-- 线程 ID 变了！
3. 操作完成 on thread 0x10e9c7000  // <-- 线程 ID 又变回来了！
```

通过这种方式，你可以将不同性质的任务隔离在不同的线程池中，防止 I/O 操作阻塞计算任务，从而极大地提升应用的响应性和吞吐量。这是构建高性能服务器和复杂应用的基石。
