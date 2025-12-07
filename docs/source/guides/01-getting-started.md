# 快速上手

本章节将指导你如何将 `koroutine_lib` 集成到你的项目中，并编写你的第一个协程程序。

## 1. 环境要求

- **C++ 编译器**: 支持 C++23 和协程。
  - GCC 11+
  - Clang 14+
  - MSVC 19.29+ (Visual Studio 2022)
- **依赖库**:
  - **Linux**: `liburing` (用于异步 I/O)
- **构建系统**: CMake 3.20+

## 2. 集成方式 (推荐)

我们推荐直接使用 Release 包中的预编译库和头文件进行集成。这种方式配置简单，编译速度快。

### 目录结构

假设你下载并解压了 Release 包，目录结构如下：

```text
project_root/
├── pkg/
│   ├── include/        # 原始头文件
│   ├── single_header/  # 单文件头文件 (推荐)
│   └── lib/            # 静态库 (libkoroutinelib.a)
├── src/
│   └── main.cpp
└── CMakeLists.txt
```

### CMake 配置

在你的 `CMakeLists.txt` 中添加以下配置：

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置包路径
set(PKG_DIR "${CMAKE_SOURCE_DIR}/pkg")

# 包含头文件 (推荐使用 single_header)
include_directories(${PKG_DIR}/single_header)

# 链接库目录
link_directories(${PKG_DIR}/lib)

add_executable(my_app src/main.cpp)

# 链接 koroutinelib
# Linux 下通常还需要链接 pthread 和 dl
target_link_libraries(my_app PRIVATE koroutinelib)
if(UNIX AND NOT APPLE)
    target_link_libraries(my_app PRIVATE pthread dl)
endif()
```

## 3. 你的第一个协程

让我们编写一个简单的程序，它启动一个协程，异步地返回一个字符串。

```cpp
// 使用单文件头文件
#include "koroutine.hpp"
#include <iostream>

using namespace koroutine;

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

编译并运行它，你会看到类似以下的输出：

```
协程开始执行 on thread 0x16f787000
// (等待1秒)
协程恢复执行
协程执行完毕，结果: Hello, from Koroutine!
```

恭喜！你已经成功运行了你的第一个 `koroutine_lib` 程序。

在下一章节，我们将深入探讨 `Task` 的更多用法。

