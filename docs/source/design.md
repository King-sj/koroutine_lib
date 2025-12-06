# 核心设计理念

`koroutine_lib` 的设计哲学是提供一组正交的、可组合的组件，以应对复杂的异步编程场景。其核心由四个主要部分构成：`Task`, `Executor`, `Scheduler`, 和 `Awaiter`。

这使得用户可以用接近同步代码的逻辑，编写出高性能、非阻塞的异步程序。

如果你对更深层次的内部实现和组件交互感兴趣，请参阅我们的 **[开发者贡献指南](../contributing/index.md)**。

## HTTP 模块设计

`koroutine_lib` 的 HTTP 模块并非从零构建，而是基于成熟的 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 库进行了深度定制和改造。

### 为什么选择改造 cpp-httplib？

1.  **成熟稳定**: `cpp-httplib` 是一个广泛使用的、单头文件的 C++ HTTP 库，功能完善且经过了大量验证。
2.  **接口友好**: 其 API 设计简洁直观，易于上手。
3.  **避免重复造轮子**: HTTP 协议细节繁多，复用现有解析逻辑可以让我们专注于异步 I/O 的适配。

### 改造的核心点

原版 `cpp-httplib` 使用阻塞式 I/O (socket API)，这与协程库的非阻塞设计背道而驰。我们的改造主要集中在以下几点：

1.  **Socket 层替换**: 将底层的阻塞 `socket` 操作替换为 `koroutine_lib` 的 `AsyncSocket`。
2.  **读写操作协程化**: 将所有的 `read`/`write` 调用改造为 `co_await socket->read(...)` 和 `co_await socket->write(...)`。
3.  **任务调度集成**: 服务器的连接处理不再是简单的线程池，而是提交给 `TaskManager` 管理的协程任务，确保了高并发下的性能和生命周期管理。

通过这种方式，我们既保留了 `cpp-httplib` 丰富的功能（如路由、参数解析、Multipart 支持等），又获得了协程带来的高性能和高并发能力。
