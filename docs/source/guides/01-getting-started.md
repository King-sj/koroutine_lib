# 快速上手

本章节将指导你完成 `koroutine_lib` 的配置、构建，并编写你的第一个协程程序。

## 1. 环境要求

- **C++ 编译器**: 支持 C++23 和协程。
  - GCC 11+
  - Clang 14+
  - MSVC 19.29+ (Visual Studio 2022)
- **构建系统**: CMake 3.20+
- **Python**: 用于文档生成 (可选)。

## 2. 构建项目

```bash
# 1. 克隆仓库
git clone https://github.com/King-sj/koroutine_lib.git
cd koroutine_lib

# 2. 配置 CMake
# -S . 指定源码根目录
# -B build 指定构建输出目录
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
cmake --build build
```

编译成功后，你会在 `build/bin` 目录下找到所有的可执行文件（demos 和 tests）。

## 3. 你的第一个协程

让我们编写一个简单的程序，它启动一个协程，异步地返回一个字符串。

```cpp
#include "koroutine/koroutine.h"
#include <iostream>

// 定义一个返回 std::string 的协程任务
Task<std::string> get_greeting() {
    std::cout << "协程开始执行 on thread " << std::this_thread::get_id() << std::endl;

    // co_await sleep_for 会挂起协程，但不会阻塞线程
    co_await sleep_for(std::chrono::seconds(1));

    std::cout << "协程恢复执行" << std::endl;
    co_return "Hello, from Koroutine!";
}

int main() {
    // Runtime::block_on 会启动一个事件循环，并运行传入的任务直到完成
    std::string result = Runtime::block_on(get_greeting());

    std::cout << "协程执行完毕，结果: " << result << std::endl;

    return 0;
}
```

将以上代码保存为 `my_first_coro.cpp`，并修改 `demos/CMakeLists.txt` 添加新的可执行目标，然后重新编译。

运行它，你会看到类似以下的输出：

```
协程开始执行 on thread 0x16f787000
// (等待1秒)
协程恢复执行
协程执行完毕，结果: Hello, from Koroutine!
```

恭喜！你已经成功运行了你的第一个 `koroutine_lib` 程序。

在下一章节，我们将深入探讨 `Task` 的更多用法。
