# 结构化并发: `when_all` 与 `when_any`

在传统的异步编程中，管理多个并发任务的生命周期是一件复杂且容易出错的事情。你可能需要手动处理回调、`std::promise`/`std::future` 对，或者复杂的事件监听。这常常导致“回调地狱”或难以追踪的资源泄漏。

**结构化并发 (Structured Concurrency)** 是一种编程范式，它要求并发任务的生命周期必须被清晰地限定在某个作用域内。`koroutine_lib` 通过 `when_all` 和 `when_any` 这两个组合器 (Combinator) 来践行这一理念。

## 1. `when_all`: 等待所有任务完成

`when_all` 接受多个 `Task`，并返回一个新的 `Task`。当 `co_await` 这个新的 `Task` 时，它会并发地执行所有传入的任务，并一直等待，直到 **所有** 任务都完成。

其结果是一个 `std::tuple`，包含了所有任务的返回值，顺序与传入时一致。

### 示例：并发下载多个文件

假设你需要同时下载三个文件，并在所有下载都完成后处理它们。

```cpp
#include "koroutine/koroutine.h"
#include <iostream>

Task<std::string> download(const std::string& url, int delay_ms) {
    std::cout << "开始下载: " << url << std::endl;
    co_await sleep_for(std::chrono::milliseconds(delay_ms));
    std::cout << "下载完成: " << url << std::endl;
    co_return "Content of " + url;
}

Task<void> main_task() {
    std::cout << "启动并发下载..." << std::endl;

    // co_await when_all 会等待所有三个 download 任务完成
    auto [content1, content2, content3] = co_await when_all(
        download("file1.zip", 1000),
        download("file2.txt", 500),
        download("file3.jpg", 1500)
    );

    std::cout << "\n所有文件下载完毕!" << std::endl;
    std::cout << "文件1: " << content1 << std::endl;
    std::cout << "文件2: " << content2 << std::endl;
    std::cout << "文件3: " << content3 << std::endl;
}

int main() {
    Runtime::run(main_task());
}
```

**输出分析：**
你会注意到，"下载完成" 的消息是乱序打印的（file2 -> file1 -> file3），这证明了它们是并发执行的。但 "所有文件下载完毕!" 这条消息一定是在所有下载都完成后才打印。

`when_all` 极大地简化了“分叉-连接 (fork-join)”模式的编码。

## 2. `when_any`: 等待任意一个任务完成

`when_any` 同样接受多个 `Task`，但它的行为不同：它会并发地执行所有任务，并一直等待，直到 **任意一个** 任务率先完成。

一旦有任务完成，`when_any` 会立即返回一个 `std::pair`，包含：
- `size_t`:率先完成的任务在输入参数中的索引。
- `T`: 该任务的结果。

> **重要**: `when_any` **不会** 自动取消其他仍在运行的任务。这些任务会继续在后台执行直到完成。

### 示例：从多个镜像源中选择最快的

假设你需要从多个镜像服务器下载同一个文件，你只关心哪个服务器最快响应。

```cpp
Task<std::string> download_from_mirror(const std::string& mirror_url, int latency) {
    co_await sleep_for(std::chrono::milliseconds(latency));
    co_return "Downloaded from " + mirror_url;
}

Task<void> main_task() {
    std::cout << "从多个镜像开始下载..." << std::endl;

    std::vector<Task<std::string>> tasks;
    tasks.push_back(download_from_mirror("mirror.a.com", 200));
    tasks.push_back(download_from_mirror("mirror.b.com", 50));
    tasks.push_back(download_from_mirror("mirror.c.com", 150));

    // 等待最快的那个任务完成
    auto [index, result] = co_await when_any(std::move(tasks));

    std::cout << "最快的服务器是 " << index << " 号!" << std::endl;
    std::cout << "结果: " << result << std::endl;
}

int main() {
    Runtime::run(main_task());
}
```

**输出：**
```
从多个镜像开始下载...
最快的服务器是 1 号!
结果: Downloaded from mirror.b.com
```

`when_any` 非常适合实现超时、冗余请求等场景。

### `when_any` 的可变参数版本

`koroutine_lib` 也提供了接受可变参数的 `when_any` 重载，其返回值是一个 `std::variant`，因为不同任务的返回类型可能不同。

```cpp
// 返回一个 std::variant<int, std::string>
auto result_variant = co_await when_any(
    task_returns_int(),
    task_returns_string()
);

// 使用 std::visit 来处理结果
std::visit([](auto&& value){
    // ...
}, result_variant);
```

通过 `when_all` 和 `when_any`，你可以用一种声明式、可读性强的方式来编排复杂的并发工作流，同时保证了任务生命周期的安全可控。
