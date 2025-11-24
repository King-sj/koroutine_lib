# 异步 I/O

现代应用程序的性能瓶颈往往在于 I/O（输入/输出）操作，如读写文件、访问数据库或进行网络通信。传统的同步 I/O 会阻塞整个线程，直到操作完成，这极大地浪费了 CPU 资源并降低了应用的吞吐量。

`koroutine_lib` 提供了一个强大的、跨平台的异步 I/O 模块——`async_io`，它将底层复杂的系统调用（如 `io_uring`, `kqueue`, `IOCP`）封装在统一、简洁的协程接口之后。

## 1. `IOEngine`: 异步 I/O 的心脏

`IOEngine` 是 `async_io` 模块的核心。它是一个在后台运行的事件循环，专门负责监听 I/O 事件。当一个异步 I/O 操作被发起时，它并不会阻塞当前协程，而是将这个操作注册到 `IOEngine`。当前协程会挂起，`IOEngine` 则在后台等待操作系统通知该操作完成。一旦完成，`IOEngine` 会负责唤醒之前挂起的协程。

`koroutine_lib` 会根据操作系统自动选择最高效的 `IOEngine` 实现：
- **Linux**: `IoUringIOEngine` (基于 `io_uring`)
- **macOS/BSD**: `KqueueIOEngine` (基于 `kqueue`)
- **Windows**: `IOCPIOEngine` (基于 `IOCP`)

你通常不需要直接与 `IOEngine` 交互，`SchedulerManager` 会为你管理一个默认的 I/O 调度器。

## 2. 异步文件操作: `AsyncFile`

`AsyncFile` 提供了对文件进行异步读写的能力。

### 打开文件

使用 `async_io::async_open` 工厂函数来异步地打开一个文件。它返回一个 `Task<std::shared_ptr<AsyncFile>>`。

```cpp
#include "koroutine/async_io/async_io.hpp"

Task<void> main_task() {
    // 异步打开文件用于写入，如果不存在则创建
    auto file = co_await async_io::async_open("my_file.txt", std::ios::out | std::ios::trunc);
    // ...
}
```

### 读写文件

`AsyncFile` 继承自 `AsyncIOObject`，提供了 `read` 和 `write` 方法。

- `co_await file->write(buffer, size)`: 异步写入数据。
- `size_t bytes_read = co_await file->read(buffer, size)`: 异步读取数据。

### 示例：异步读取并写入文件

下面的例子展示了如何异步地将一个文件的内容复制到另一个文件。

```cpp
#include "koroutine/async_io/async_io.hpp"
#include <vector>
#include <iostream>

Task<void> async_copy_file(const std::string& src_path, const std::string& dest_path) {
    try {
        auto src_file = co_await async_io::async_open(src_path, std::ios::in);
        auto dest_file = co_await async_io::async_open(dest_path, std::ios::out | std::ios::trunc);

        std::vector<char> buffer(4096);
        while (true) {
            // 异步读取源文件
            size_t bytes_read = co_await src_file->read(buffer.data(), buffer.size());
            if (bytes_read == 0) {
                break; // 文件读取完毕
            }
            // 异步写入目标文件
            co_await dest_file->write(buffer.data(), bytes_read);
        }
        std::cout << "文件复制成功!" << std::endl;
    } catch (const std::system_error& e) {
        std::cerr << "文件操作失败: " << e.what() << std::endl;
    }
}

int main() {
    // 确保你的程序启动了 IO 调度器
    SchedulerManager::create_io_scheduler();
    Runtime::block_on(async_copy_file("input.txt", "output.txt"));
}
```

## 3. 异步网络操作: `AsyncSocket`

`AsyncSocket` 提供了进行异步网络编程的能力，包括建立连接、发送和接收数据。

### 建立连接

使用 `async_io::async_connect` 来异步地连接到一个 TCP 服务器。

```cpp
Task<void> connect_to_server() {
    try {
        // 异步连接到本地 8080 端口
        auto socket = co_await async_io::async_connect("127.0.0.1", 8080);
        std::cout << "成功连接到服务器!" << std::endl;
        // ...
    } catch (const std::system_error& e) {
        std::cerr << "连接失败: " << e.what() << std::endl;
    }
}
```

### 示例：一个简单的 HTTP 客户端

下面是一个使用 `AsyncSocket` 异步发送一个 HTTP GET 请求并接收响应的例子。

```cpp
#include "koroutine/async_io/async_io.hpp"
#include <vector>
#include <iostream>

Task<void> http_get_example(const std::string& host, uint16_t port, const std::string& path) {
    try {
        auto socket = co_await async_io::async_connect(host, port);
        std::cout << "成功连接到 " << host << std::endl;

        std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";

        // 异步发送请求
        co_await socket->write(request.c_str(), request.length());
        std::cout << "HTTP 请求已发送" << std::endl;

        std::vector<char> response_buffer(4096);
        // 异步接收响应
        size_t bytes_read = co_await socket->read(response_buffer.data(), response_buffer.size() - 1);

        response_buffer[bytes_read] = '\0'; // 添加 null 终止符
        std::cout << "\n收到响应:\n" << response_buffer.data() << std::endl;

    } catch (const std::system_error& e) {
        std::cerr << "HTTP 请求失败: " << e.what() << std::endl;
    }
}

int main() {
    SchedulerManager::create_io_scheduler();
    Runtime::block_on(http_get_example("example.com", 80, "/"));
}
```

通过 `async_io` 模块，你可以用同步风格的代码编写出高性能的、完全非阻塞的 I/O 密集型应用程序，例如网络爬虫、HTTP 服务器、数据库代理等。
