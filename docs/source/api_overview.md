# API 概览

此页面为快速索引，列出仓库中对外最常用的 API、所在头文件与简要说明，方便快速查找。

- `Task<T>` — `include/koroutine/task.hpp`
  - 惰性、可等待的异步任务类型。常用方法：`start()`, `then()`, `catching()`, `finally()`。

- `SchedulerManager` — `include/koroutine/scheduler_manager.h` / `src/scheduler_manager.cpp`
  - 全局默认调度器管理。常用函数：`get_default_scheduler()`, `set_default_scheduler()`。

- `AbstractExecutor` — `include/koroutine/executors/executor.h`
  - 执行器抽象（`execute`, `execute_delayed`）。实现：`LooperExecutor`, `NewThreadExecutor`, `AsyncExecutor`。

- `AbstractScheduler` / `ScheduleRequest` — `include/koroutine/schedulers/scheduler.h`, `include/koroutine/schedulers/schedule_request.hpp`
  - 调度请求封装 `std::coroutine_handle<>` 与 `ScheduleMetadata`（优先级/亲和性/调试名称）。

- `Channel<T>` — `include/koroutine/channel.hpp`
  - 协程间的异步通信队列，提供 `write()` / `read()` awaitable。

- `async_io` — `include/koroutine/async_io/` (工厂：`async_io/async_io.hpp`)
  - 跨平台异步 IO 抽象（`AsyncFile`, `AsyncSocket` 等），根据平台选择不同引擎（`io_uring`, `kqueue`, `IOCP`）。

- `HTTP` — `include/koroutine/async_io/httplib.h`
  - 高性能异步 HTTP Server 和 Client。详见 [HTTP 模块](./http.md)。

- `Runtime` — `include/koroutine/runtime.hpp`
  - 同步运行协程的入口：`Runtime::block_on`, `Runtime::join_all` 等。

- `Awaiters` — `include/koroutine/awaiters/`
  - 常用 awaiters: `sleep_awaiter`, `task_awaiter`, `switch_executor_awaiter`, `io_awaiter`, `channel_awaiter`。

- `CancellationToken` — `include/koroutine/cancellation.hpp`
  - 协作式取消支持。

更多详细 API 可通过源码直接查看对应头文件（本仓库已包含 `Doxygen` 支持用于更深入的自动化 API 文档）。

