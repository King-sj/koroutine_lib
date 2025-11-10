# 生成文档与依赖图（Doxygen + Graphviz）

本项目已添加对 Doxygen + Graphviz 的支持，用于生成更详细的依赖图（头文件包含图、函数调用图、类依赖图）。

前提依赖：
- Doxygen
- Graphviz (dot)

在 macOS 上可以通过 Homebrew 安装：

```bash
brew install doxygen graphviz
```

使用方法：

1. 在构建目录配置 CMake（若尚未配置）：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

2. 生成项目并触发文档生成目标：

```bash
cmake --build build --target doc
```

或者进入 `build` 目录运行 `make doc`（取决于生成器）。

输出位置：
- HTML 文档与图像将输出到 `build/docs/html`（默认），图像一般为 SVG 格式以保留可缩放与交互性。

说明：
- Doxygen 会自动生成函数调用图（CALL_GRAPH / CALLER_GRAPH）、类关系图（CLASS_DIAGRAMS）以及包含关系图（INCLUDE_GRAPH）。
- 若生成失败或没有图，确认 Doxygen 与 Graphviz 是否安装且在 PATH 中。若需要更细粒度控制（例如只针对某些目录或文件生成），请编辑 `Doxyfile.in` 并重新运行 CMake/`doc` 目标。
