# 通道 (Channel): 协程间的安全通信

当多个协程并发执行时，你很快就会遇到一个经典问题：如何在它们之间安全、高效地传递数据？直接共享内存和使用锁（Mutex）是一种方法，但这很容易出错，并且可能导致死锁或性能瓶颈。

`koroutine_lib` 提供了 `Channel<T>`，这是一种受 Go 语言启发的并发原语，专门用于在协程之间进行通信。

## 1. 什么是 `Channel`？

`Channel` 可以被看作是一个线程安全的、阻塞的消息队列。你可以将它想象成一根管道：
- **生产者 (Producer)** 协程可以将数据写入管道的一端。
- **消费者 (Consumer)** 协程可以从管道的另一端读取数据。

这个过程是完全由协程驱动的，并且是“异步阻塞”的：
- 如果管道已满，生产者协程在尝试写入时会自动 **挂起**（而不是阻塞线程），直到管道中有空间。
- 如果管道为空，消费者协程在尝试读取时会自动 **挂起**，直到管道中有新的数据。

## 2. `Channel` 的基本用法

### 创建 `Channel`

你可以创建一个 `Channel` 并指定其容量。容量决定了在生产者被挂起之前，可以缓冲多少个元素。

```cpp
// 创建一个容量为 10 的整数通道
auto channel = std::make_shared<Channel<int>>(10);

// 创建一个无缓冲通道 (容量为 0)
// 这种通道要求生产者和消费者必须同时准备好，否则一方会挂起
auto sync_channel = std::make_shared<Channel<int>>(0);
```

### 写入和读取

- `co_await channel->write(value)`: 异步地向通道写入一个值。
- `ValueType value = co_await channel->read()`: 异步地从通道读取一个值。

### 示例：生产者-消费者模型

下面是一个经典的生产者-消费者例子。一个协程生成数字并放入通道，另一个协程从通道中取出数字并处理。

```cpp
#include "koroutine/koroutine.h"
#include <iostream>

// 生产者协程
Task<void> producer(std::shared_ptr<Channel<int>> channel) {
    for (int i = 0; i < 5; ++i) {
        std::cout << "Producer: 发送 " << i << std::endl;
        co_await channel->write(i); // 写入数据，如果通道满了会挂起
        co_await sleep_for(std::chrono::milliseconds(100)); // 模拟生产耗时
    }
    std::cout << "Producer: 完成发送，关闭通道" << std::endl;
    channel->close(); // 关闭通道，通知消费者不会再有新数据
}

// 消费者协程
Task<void> consumer(std::shared_ptr<Channel<int>> channel) {
    try {
        while (true) {
            // 读取数据，如果通道空了会挂起
            int value = co_await channel->read();
            std::cout << "Consumer: 收到 " << value << std::endl;
        }
    } catch (const Channel<int>::ChannelClosedException&) {
        // 当通道被关闭且为空时，read() 会抛出异常
        std::cout << "Consumer: 通道已关闭，退出" << std::endl;
    }
}

int main() {
    auto channel = std::make_shared<Channel<int>>(2); // 容量为 2

    // 并发运行生产者和消费者
    Runtime::block_on(when_all(
        producer(channel),
        consumer(channel)
    ));

    return 0;
}
```

**输出分析：**
```
Producer: 发送 0
Consumer: 收到 0
Producer: 发送 1
Consumer: 收到 1
Producer: 发送 2
Producer: 发送 3  // 此时通道满了 (容量为2)，生产者在 write(3) 后会挂起
Consumer: 收到 2  // 消费者取走一个，通道有空间了
Producer: 发送 4  // 生产者被唤醒，继续发送 3，然后发送 4
Consumer: 收到 3
...
```

## 3. 关闭 `Channel`

当生产者不再发送数据时，应该调用 `channel->close()` 来关闭通道。这是一个重要的信号，用于通知消费者可以停止等待。

- 关闭通道后，任何新的 `write` 操作都会立即抛出 `ChannelClosedException`。
- 如果通道中仍有缓冲的数据，消费者可以继续 `read` 直到通道变空。
- 当一个已关闭且已变空的通道被 `read` 时，会立即抛出 `ChannelClosedException`，这是消费者优雅退出的标准方式。

`Channel` 是构建复杂并发工作流（如扇入、扇出、流水线）的强大基础模块，它将跨线程通信的复杂性隐藏在简单的 `co_await` 语法背后。
