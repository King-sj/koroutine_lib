# Koroutine 重构 / 功能 TODO 列表

- 添加 switch_executor awaiter：支持在协程中切换 executor 并在新 executor 上 resume。
3. 实现 when_all / when_any 等并发组合器，使多个 Task 能并发等待并聚合结果。
4. 为 via、switch_executor、sleep-via-executor 等新特性添加单元测试。
5. 更新 README / docs / 使用示例，说明新的 executor 绑定模型和 API（via、switch_executor、ExecutorManager）。
7. 在 CI 中运行完整构建与测试。

- [ ] std::task ：通用异步任务类型，支持co_await等待结果，可组合多个任务（如when_all/when_any），替代手动实现的异步任务框架。
- [ ] 与std::future的整合：允许co_await直接等待std::future，消除两者的割裂
- [ ] 清晰的职责分离:
    - [ ] Executor: 只管执行。
    - [ ] Scheduler/Reactor: 处理时间事件和 I/O 事件。通常会有一个全局的或线程局部的事件循环。
    - [ ] Task/Promise: 连接协程状态和用户代码。
    - [ ] Awaiter: 作为 co_await 的操作对象，封装挂起和恢复的逻辑。
    - [ ] 结构化并发 (Structured Concurrency):
- [ ] 提供 coroutine_scope 或类似机制，确保在一个作用域内启动的所有协程都在离开该作用域前完成或被取消。这能极大地避免资源泄漏和僵尸任务，是现代异步框架（如 Swift, Kotlin）的标志性特性。
取消机制 (Cancellation):
- [ ] 提供一套标准的、协作式的取消机制。当一个协程任务不再需要时，可以安全地通知它停止工作并释放资源。
- [ ] 优先级调度
- [ ] awaitable list
    - [ ] await gather(list of awaitables)
- [ ] conditonal varialble 支持条件等待