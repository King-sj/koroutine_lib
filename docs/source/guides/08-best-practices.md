# 最佳实践（Best Practices）

本节给出在使用 `koroutine_lib` 时的若干建议，覆盖协程生命周期、取消、调度器选择、性能调优与调试技巧。

## 1. 协程生命周期与结构化并发

- 优先使用 `when_all` / `Runtime::join_all` 等结构化并发原语来限定协程的生命周期，避免“火箭发射器式”的后台任务（无控制的长期驻留任务）。
- 对于短生命周期的辅助任务，使用 `details::FireAndForget` 或者明确 `start()` 并保证在作用域退出前有合适的取消/等待逻辑。

## 2. 取消（Cancellation）

- 使用库提供的 `CancellationToken`（见 `include/koroutine/cancellation.hpp`）进行合作式取消，而不是强行终止线程。
- 在可能长时间挂起的 awaiter（IO/睡眠/Channel）处检测取消标志并尽早返回。

## 3. 调度器与执行器选择

- CPU 密集型任务应当提交到计算线程池（可通过自建 `AsyncExecutor` 或类似机制），I/O 密集型任务优先使用 `IO Scheduler`（默认通过 `SchedulerManager` 提供）。
- 对于需要线程亲和性（如访问非线程安全资源或 UI 更新），使用 `LooperExecutor` 并通过 `co_await switch_to(looper_executor)` 切换到该上下文。


## 4. 性能分析 (Profiling)

为了确保你的协程应用达到最佳性能，建议定期进行性能分析。

### 构建配置

在进行性能分析之前，请确保使用 `RelWithDebInfo` 模式构建项目。这既能开启优化，又能保留调试符号，方便定位热点函数。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo
```

### macOS 平台分析工具

在 macOS 上，你可以使用以下工具：

1.  **Instruments (Time Profiler)**:
    Xcode 自带的强大图形化分析工具。
    *   打开 Instruments (`Cmd+I` in Xcode, or open separately).
    *   选择 "Time Profiler".
    *   选择你的可执行文件并开始录制。
    *   分析调用栈，找出耗时最长的函数。

2.  **`sample` 命令行工具**:
    如果你只需要快速查看运行中进程的采样：
    ```bash
    # 假设你的程序 PID 是 12345
    sample 12345 10 -f output.txt
    ```
    这会采样 10 秒并将结果输出到 `output.txt`。

### Linux 平台分析工具

在 Linux 上，`perf` 是最常用的工具：

```bash
# 记录性能数据
perf record -g ./your_application

# 查看分析报告
perf report
```

通过火焰图 (FlameGraph) 可以更直观地查看协程调度和业务逻辑的开销分布。


- 减少频繁的 `start()`/`resume()` 切换，尽量把短任务合并在同一协程内完成；
- 避免在执行器线程内做大量阻塞操作，除非该执行器专门用于阻塞 I/O；
- 对于高频调用路径，考虑减少堆分配和 std::function 的动态分配（可以在未来用 folly/boost 的小函数优化替代）；

## 5. 调试与日志

- 启用 `KOROUTINE_DEBUG` 宏可以打印更详细的运行时信息（见 `include/koroutine/debug.h`）；
- 使用 `ScheduleMetadata::debug_name` 在调度点传递可读的任务标识，以便在日志中归因。

## 6. API 兼容性与版本策略

- 如果你计划将来支持 C++26 的 `std::execution` 能力，建议在库中增加适配层（adapter），而不是把内部实现替换为 `std::execution`。
- 保持 `Task`/`Awaiter` 的 API 稳定，避免在 minor 版本中破坏调用约定。

