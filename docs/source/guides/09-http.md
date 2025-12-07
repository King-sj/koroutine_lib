# HTTP 模块

`koroutine_lib` 提供了一个基于 `cpp-httplib` 的异步 HTTP 模块。它保留了 `cpp-httplib` 简洁易用的 API 风格，同时利用协程实现了全异步的 I/O 操作。

## 核心特性

- **全异步 I/O**: 基于 `koroutine` 的调度器和异步 I/O 引擎，避免线程阻塞。
- **兼容性**: API 设计尽可能兼容 `cpp-httplib`，降低学习成本。
- **高性能**: 结合协程的高效调度，适合高并发场景。

## HTTP Server

### 基本用法

创建一个异步 HTTP 服务器非常简单。你需要定义路由处理函数，这些函数现在返回 `koroutine::Task<void>` 并可以包含 `co_await` 调用。

```cpp
#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server() {
    // 创建 Server 实例 (使用 shared_ptr 是为了在协程中保持生命周期，视具体实现而定)
    auto svr = std::make_shared<Server>();

    // 定义一个简单的 GET 路由
    svr->Get("/hi", [](const Request& req, Response& res) -> Task<void> {
        res.set_content("Hello World!", "text/plain");
        co_return;
    });

    // 异步监听端口
    std::cout << "Listening on 0.0.0.0:8080..." << std::endl;
    bool ret = co_await svr->listen_async("0.0.0.0", 8080);
    if (!ret) {
        std::cerr << "Failed to listen" << std::endl;
    }
}

int main() {
    // 启动运行时
    Runtime::block_on(run_server());
    return 0;
}
```

### 路由与参数

支持 `cpp-httplib` 的所有路由方式，包括正则匹配。

```cpp
svr->Get(R"(/users/(\d+))", [](const Request& req, Response& res) -> Task<void> {
    auto user_id = req.matches[1];
    // 模拟异步数据库查询
    // auto user = co_await db.get_user(user_id);
    res.set_content("User info for " + std::string(user_id), "text/plain");
    co_return;
});

svr->Post("/echo", [](const Request& req, Response& res) -> Task<void> {
    res.set_content(req.body, "text/plain");
    co_return;
});
```

### 异步处理

在 Handler 中，你可以自由调用其他异步操作，例如读取文件、请求其他服务或休眠。

```cpp
svr->Get("/sleep", [](const Request& req, Response& res) -> Task<void> {
    // 非阻塞休眠 1 秒
    co_await koroutine::sleep_for(std::chrono::seconds(1));
    res.set_content("Woke up!", "text/plain");
});
```

### 请求与响应详解

#### 获取请求头与参数

```cpp
svr->Get("/headers", [](const Request& req, Response& res) -> Task<void> {
    // 获取 Header
    if (req.has_header("User-Agent")) {
        auto val = req.get_header_value("User-Agent");
        std::cout << "User-Agent: " << val << std::endl;
    }

    // 获取 Query Param (e.g. /headers?id=123)
    if (req.has_param("id")) {
        auto val = req.get_param_value("id");
    }

    co_return;
});
```

#### 设置响应状态与头

```cpp
svr->Get("/custom", [](const Request& req, Response& res) -> Task<void> {
    res.status = 418; // I'm a teapot
    res.set_header("X-Custom-Header", "Koroutine");
    res.set_content("Custom response", "text/plain");
    co_return;
});
```

#### 文件上传 (Multipart)

处理 `multipart/form-data` 上传的文件：

```cpp
svr->Post("/upload", [](const Request& req, Response& res) -> Task<void> {
    if (req.has_file("file")) {
        const auto& file = req.get_file_value("file");

        std::cout << "Filename: " << file.filename << std::endl;
        std::cout << "Content-Type: " << file.content_type << std::endl;
        std::cout << "File size: " << file.content.size() << std::endl;

        // 可以在这里异步写入磁盘
        // co_await async_write_file(file.filename, file.content);
    }
    res.set_content("Upload success", "text/plain");
    co_return;
});
```

### 静态文件服务

你可以将本地目录挂载到服务器路径上，提供静态文件服务。

```cpp
// 将当前目录下的 www 文件夹挂载到 /public 路径
auto ret = svr->set_mount_point("/public", "./www");
if (!ret) {
    // 目录不存在
}
```

### 日志记录

支持设置访问日志和错误日志。注意 Logger 回调是同步执行的，应避免在其中执行耗时操作。

```cpp
svr->set_logger([](const Request& req, const Response& res) {
    std::cout << req.method << " " << req.path << " -> " << res.status << std::endl;
});
```

### 错误处理

你可以自定义错误页面（如 404 Not Found）。

```cpp
svr->set_error_handler([](const Request& req, Response& res) -> Task<void> {
    res.set_content("Custom Error Page", "text/plain");
    co_return;
});
```

### 异常处理

如果路由处理函数中抛出异常，会调用异常处理器。

```cpp
svr->set_exception_handler([](const Request& req, Response& res, std::exception_ptr ep) {
    res.status = 500;
    try {
        std::rethrow_exception(ep);
    } catch (std::exception &e) {
        res.set_content(e.what(), "text/plain");
    } catch (...) {
        res.set_content("Unknown Exception", "text/plain");
    }
});
```

### 路由钩子

支持在路由处理前后执行钩子函数。

```cpp
// 路由前钩子
svr->set_pre_routing_handler([](const Request& req, Response& res) {
    if (req.path == "/hello") {
        res.set_content("world", "text/html");
        return Server::HandlerResponse::Handled;
    }
    return Server::HandlerResponse::Unhandled;
});

// 路由后钩子
svr->set_post_routing_handler([](const Request& req, Response& res) -> Task<void> {
    res.set_header("X-Powered-By", "Koroutine");
    co_return;
});
```

## HTTP Client

客户端同样支持异步调用。

### 发送请求

```cpp
Task<void> fetch_url() {
    Client cli("http://localhost:8080");

    // 异步发送 GET 请求
    auto res = co_await cli.Get("/hi");

    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
    } else {
        std::cout << "Request failed" << std::endl;
    }
}
```

### 更多请求方式

支持 POST, PUT, DELETE 等常见方法，以及自定义 Header 和 Body。

```cpp
Task<void> post_data() {
    Client cli("http://localhost:8080");

    // POST JSON 数据
    auto res = co_await cli.Post("/api/data",
        R"({"key": "value"})",
        "application/json"
    );

    // 带 Header 的请求
    Headers headers = {
        {"Authorization", "Bearer token123"}
    };
    auto res2 = co_await cli.Get("/protected", headers);
}
```

### 错误处理

检查 `res->error()` 来获取详细的错误信息。

```cpp
auto res = co_await cli.Get("/hi");
if (!res) {
    auto err = res.error();
    std::cout << "HTTP error: " << httplib::to_string(err) << std::endl;
}
```

### 设置超时

你可以为 Client 设置连接和读写超时。

```cpp
Client cli("http://localhost:8080");
cli.set_connection_timeout(std::chrono::seconds(2)); // 连接超时
cli.set_read_timeout(std::chrono::seconds(5));       // 读取超时
cli.set_write_timeout(std::chrono::seconds(5));      // 写入超时
```

### 进度回调

支持上传和下载的进度回调。

```cpp
auto res = co_await cli.Get("/", [](size_t len, size_t total) {
    printf("%lld / %lld bytes => %d%% complete\n",
        len, total,
        (int)(len*100/total));
    return true; // 返回 false 取消请求
});
```

### 认证

支持 Basic, Digest 和 Bearer Token 认证。

```cpp
// Basic Authentication
cli.set_basic_auth("user", "pass");

// Digest Authentication
cli.set_digest_auth("user", "pass");

// Bearer Token Authentication
cli.set_bearer_token_auth("token");
```

### 代理支持

```cpp
cli.set_proxy("host", port);
cli.set_proxy_basic_auth("user", "pass");
```

### 并发请求

利用协程，你可以轻松并发多个 HTTP 请求。

```cpp
Task<void> fetch_many() {
    Client cli("http://localhost:8080");

    // 启动多个任务
    // 注意：这里需要确保 Client 实例在所有任务完成前有效，或者每个任务使用独立的 Client
    // 如果 Client 是线程安全的或支持并发请求（取决于实现），可以共享。
    // 通常建议每个协程或请求使用独立的 Client，或者使用连接池。

    // 简单示例：顺序发起但异步等待
    for(int i=0; i<10; ++i) {
        auto res = co_await cli.Get("/hi");
        // 处理结果
    }
}
```

## 与 cpp-httplib 的区别

虽然 API 相似，但有以下关键区别：

1.  **Handler 返回值**: 必须返回 `Task<void>`。
2.  **监听方式**: 使用 `listen_async` 替代 `listen`。
3.  **客户端调用**: `Get`/`Post` 等方法返回 `Awaitable`，需要 `co_await`。
4.  **线程模型**: 运行在 `koroutine` 的 Executor 上，而不是每个连接一个线程。这使得它能以极少的线程处理大量并发连接。

## 更多文档

`koroutine_lib` 的 HTTP 模块 API 设计与 `cpp-httplib` 高度一致。

关于更多高级用法（如 SSL/TLS 配置、分块传输、Range 请求等），请直接参考 **[cpp-httplib 官方文档](https://github.com/yhirose/cpp-httplib)**。

只要将原文档中的同步调用改为 `co_await` 异步调用，大部分示例代码都可以直接使用。
