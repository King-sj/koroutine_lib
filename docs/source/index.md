# 欢迎来到 Koroutine Library

[`koroutine lib`](https://github.com/king-sj/koroutine_lib/) 是一个基于 C++23 的现代化协程库，专为结构化并发和高性能异步 I/O 设计。

本项目文档旨在提供全面的指南，帮助你理解其核心概念、API 并将其集成到你的项目中。

## 从哪里开始

- **如果你是新用户**，请从 **[核心设计](./design.md)** 开始，了解库的基本架构。
- **如果你想立刻开始编码**，请访问 **[使用指南](./guides/01-getting-started.md)**。

## 致谢

`koroutine_lib` 的 HTTP 模块深度集成了 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 项目。我们对其进行了协程化改造，使其能够无缝运行在 `koroutine_lib` 的异步 I/O 框架之上。感谢原作者 yhirose 提供的优秀基础。
