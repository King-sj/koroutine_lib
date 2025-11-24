# Koroutine 重构 / 功能 TODO 列表
4. 为 via、switch_executor、sleep-via-executor 等新特性添加单元测试。
5. 更新 README / docs / 使用示例，说明新的 executor 绑定模型和 API（via、switch_executor、ExecutorManager）。
7. 在 CI 中运行完整构建与测试。

- [ ] 与std::future的整合：允许co_await直接等待std::future，消除两者的割裂
- [ ] 提供 coroutine_scope 或类似机制，确保在一个作用域内启动的所有协程都在离开该作用域前完成或被取消。这能极大地避免资源泄漏和僵尸任务，是现代异步框架（如 Swift, Kotlin）的标志性特性。
- [ ] 提供一套标准的、协作式的取消机制。当一个协程任务不再需要时，可以安全地通知它停止工作并释放资源。
- [ ] 优先级调度
- [ ] awaitable list
    - [ ] await gather(list of awaitables)
- [ ] 使用 std::expected 替代异常处理
- [ ] 提供协程调试工具
    - [ ] 堆栈跟踪
    - [ ] 任务状态监控
- [ ] 性能优化
    - [ ] 减少内存分配
    - [ ] 减少上下文切换
    - [ ] 提高缓存命中率
- [ ] 支持协程间通信机制
    - [x] 通道 (Channels)
    - [ ] 消息队列 (Message Queues)
- [ ] 支持协程本地存储 (Coroutine Local Storage)
    - [ ] 允许在协程中存储和访问局部数据
- [ ] 提供协程优先级支持
    - [ ] 允许为协程设置优先级，以影响调度顺序
- [ ] 增强与现有异步库的互操作性
    - [ ] 提供适配器，使得现有基于回调或 future 的异步库能够无缝集成到协程框架中

# --- C++26 兼容性与未来规划 ---

- [ ] **拥抱 C++26 `<debugging>`**
  - [ ] 创建 `include/koroutine/std_debug.h` 垫片。
  - [ ] 实现 `KOROUTINE_BREAKPOINT()` 宏，在支持时使用 `std::breakpoint()`。
  - [ ] 在 `LOG_ERROR` 中集成 `std::is_debugger_present()`。

- [ ] **适配 `std::execution::scheduler`**
  - [ ] 创建 `StdExecutionSchedulerAdapter`，包装 `AbstractScheduler`，使其满足 `scheduler` concept。
  - [ ] 实现 `ScheduleSender`，作为 `StdExecutionSchedulerAdapter::schedule()` 的返回值。

- [ ] **实现 Sender/Task 互操作性**
  - [ ] 增加 `await_transform` 重载，使 `Task` 可以 `co_await` 任何 `std::execution::sender`。
  - [ ] 为 `Task<T>` 实现 `connect` 接口或 `as_sender()` 转换，使其自身成为一个 `Sender`。

- [ ] **采用现代 C++ 特性**
  - [ ] 迁移到 `std::expected` (C++23) 进行错误处理，替代部分异常。
  - [ ] 评估 `std::lazy` (C++26) 与当前 `Task` 的异同，保持设计理念一致。
