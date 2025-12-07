# 欢迎来到 Koroutine Library

<div align="center">
  <p>
    <a href="https://github.com/King-sj/koroutine_lib">
      <img src="https://img.shields.io/badge/GitHub-Repository-black?logo=github" alt="GitHub Repository">
    </a>
    <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
    <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
  </p>

  <p><strong>专为 C++23 设计的现代化、高性能协程库</strong></p>
</div>

`koroutine_lib` 是一个基于 C++23 的现代化协程库，专为结构化并发和高性能异步 I/O 设计。它旨在简化异步编程，让你用同步的代码风格编写高效的异步程序。

## 核心特性

- 🚀 **高性能**: 基于 `io_uring` (Linux) 和原生异步 API，提供极致的 I/O 性能。
- 🧵 **结构化并发**: 提供 `Task`, `Generator`, `Channel` 等原语，轻松管理协程生命周期。
- 🌐 **异步 HTTP**: 内置基于 `cpp-httplib` 的全异步 HTTP 客户端与服务端。
- 🧩 **易于集成**: 提供单文件头文件 (`single_header`) 和预编译库，开箱即用。

## 快速链接

<div class="grid cards" markdown>

-   :material-github: **源码仓库**
    ---
    访问 GitHub 仓库获取最新源码、提交 Issue 或贡献代码。

    [前往 GitHub :arrow_right:](https://github.com/King-sj/koroutine_lib/)

-   :material-school: **快速上手**
    ---
    学习如何配置环境并编写你的第一个协程程序。

    [开始学习 :arrow_right:](./guides/01-getting-started.md)

-   :material-book-open-page-variant: **核心设计**
    ---
    深入了解库的架构设计与实现原理。

    [阅读设计文档 :arrow_right:](./guides/00-core-design.md)

-   :material-web: **HTTP 模块**
    ---
    构建高性能的异步 HTTP 服务。

    [查看文档 :arrow_right:](./guides/09-http.md)

</div>

## 致谢

`koroutine_lib` 的 HTTP 模块深度集成了 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 项目。我们对其进行了协程化改造，使其能够无缝运行在 `koroutine_lib` 的异步 I/O 框架之上。感谢原作者 yhirose 提供的优秀基础。

