# HTTP 模块

`koroutine_lib` 提供了一个基于协程的高性能 HTTP 服务器和客户端实现。该模块基于著名的 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 库进行了深度改造，将其底层的同步 Socket I/O 替换为 `koroutine` 的异步 I/O (`AsyncSocket`)，从而实现了真正的非阻塞、高并发处理能力。

## 特性

- **全异步非阻塞**: 基于 `iouring` (Linux) / `kqueue` (macOS) / `IOCP` (Windows) 的异步 I/O。
- **高性能**:
  - 优化的 `BufferedStream` 实现，大幅减少系统调用和协程切换开销。
  - 支持 Keep-Alive 连接复用。
- **易用性**: 保持了 `cpp-httplib` 简洁直观的 API 设计。
- **功能丰富**:
  - 支持 HTTP/1.1。
  - 支持 GET, POST, PUT, DELETE 等所有标准方法。
  - 支持 Chunked Transfer Encoding。
  - 支持 Multipart Form Data。
  - 支持 SSL/TLS (需要 OpenSSL)。

## HTTP Server

### 基本用法

```cpp
#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server() {
  auto svr = std::make_shared<Server>();

  // 定义路由处理函数
  svr->Get("/hi", [](const Request& req, Response& res) -> Task<void> {
    res.set_content("Hello World!", "text/plain");
    co_return;
  });

  // 处理带参数的路由
  svr->Get(R"(/users/(\d+))", [](const Request& req, Response& res) -> Task<void> {
    auto user_id = req.matches[1];
    res.set_content("User ID: " + std::string(user_id), "text/plain");
    co_return;
  });

  std::cout << "Server listening on http://0.0.0.0:8080" << std::endl;

  // 异步启动监听
  // listen_async 是一个协程，会一直运行直到服务器停止
  bool ret = co_await svr->listen_async("0.0.0.0", 8080);

  if (!ret) {
    std::cerr << "Failed to listen" << std::endl;
  }
}

int main() {
  Runtime::block_on(run_server());
  return 0;
}
```

### 性能优化

HTTP Server 内部使用了 `BufferedStream` 来优化读取性能。对于包含大量 Header 的请求，这可以将处理时间从秒级降低到毫秒级。

## HTTP Client

### 基本用法

```cpp
#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_client() {
  // 创建客户端
  Client cli("http://localhost:8080");

  // 发送 GET 请求
  auto res = co_await cli.Get("/hi");

  if (res) {
    std::cout << "Status: " << res->status << std::endl;
    std::cout << "Body: " << res->body << std::endl;
  } else {
    auto err = res.error();
    std::cout << "Error: " << to_string(err) << std::endl;
  }

  // 发送 POST 请求
  auto res_post = co_await cli.Post("/post", "body data", "text/plain");
}
```

### 并发请求

利用 `koroutine` 的结构化并发特性，可以轻松发起并发请求：

```cpp
Task<void> concurrent_requests() {
  Client cli("http://localhost:8080");

  // 同时发起两个请求
  auto [res1, res2] = co_await when_all(
    cli.Get("/hi"),
    cli.Get("/users/123")
  );

  // 处理结果...
}
```

## 注意事项

1. **生命周期**: 确保 `Server` 和 `Client` 对象在异步操作期间保持存活。通常建议使用 `std::shared_ptr` 或在主协程栈上分配。
2. **异常处理**: 所有的 Handler 都应该是非阻塞的。如果需要执行耗时的 CPU 密集型任务，建议将其调度到专门的线程池中执行，以免阻塞 I/O 线程。

## 示例代码

在 `demos/http/` 目录下提供了丰富的示例代码：

- `simple_server.cpp`: 基础的 HTTP 服务器。
- `simple_client.cpp`: 基础的 HTTP 客户端。
- `file_server.cpp`: 静态文件服务器。
- `upload_server.cpp`: 文件上传服务器。
- `benchmark.cpp`: 简单的性能测试工具。

你可以通过以下命令运行这些示例：

```bash
./build/bin/file_server_demo
./build/bin/upload_server_demo
```
