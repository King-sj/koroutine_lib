# 工具使用指南

## Task Checker

`task_checker` 是一个用于检测 C++ 代码中常见协程错误的工具，特别是检测 `Task` 对象是否被正确 `co_await`。

~~笔者在支持 http 时，直接通过模块 cpp-httplib 实现的（他是一个 blocking 的 http 库）, 导致出现了大量的直接返回 Task 对象但没有 co_await 的错误，最终导致协程无法正确执行。为了解决这个问题，编写了 `task_checker` 工具来自动检测这些错误。~~

### 安装

该工具位于 `tools/task_checker` 目录下。你需要安装 Python 3.10+ 和 `uv` 包管理器。

```bash
cd tools/task_checker
uv venv
source .venv/bin/activate
uv pip install .
```

### 使用方法

运行 `check_tasks.py` 脚本，并指定要检查的文件或目录。

```bash
# 检查单个文件
python check_tasks.py ../../tests/unit/test_http.cpp --compile-db ../../build

# 检查整个目录（递归查找 .cpp 文件）
python check_tasks.py ../../src --compile-db ../../build
```

### 参数说明

- `files`: 要检查的 C++ 源文件或目录。
- `--compile-db`: 包含 `compile_commands.json` 的目录路径（通常是构建目录）。这对于正确解析包含路径非常重要。
- `--libclang-path`: 指定 `libclang` 库文件的路径（如果自动检测失败）。

### 常见错误

工具会报告以下类型的错误：

1.  **Task returned by function call is ignored**: 函数调用返回了一个 `Task`，但没有被 `co_await` 或存储。这通常意味着协程不会被执行。
2.  **co_return returning a Task directly**: 在协程中使用 `co_return task;` 而不是 `co_return co_await task;`。这可能导致嵌套的 Task 类型。

### 示例输出

```
Checking /path/to/koroutine_lib/tests/unit/test_http.cpp...

Violations in test_http.cpp
```

## Upstream Update Checker

由于 `koroutine` 的 HTTP 模块深度参考了 `cpp-httplib`，为了保持与上游的同步，我们提供了一个自动检查脚本。

### 工作原理

脚本 `scripts/check_httplib_updates.py` 会对比 `cpp-httplib` 仓库的最新提交与本地记录的上次同步点（`.httplib_last_sync`）。如果发现新提交，脚本会列出它们并以非零状态码退出，从而中断构建过程，提醒开发者关注。

### 如何处理更新

当构建因为发现上游更新而失败时：

1.  运行脚本查看更新内容：
    ```bash
    python3 scripts/check_httplib_updates.py
    ```
2.  分析列出的 Commit，决定是否需要移植到 `koroutine`。
3.  如果决定移植，请参考上游修改手动应用到 `include/koroutine/async_io/httplib.h`。
4.  处理完毕后（或决定忽略），更新同步点：
    ```bash
    # 脚本输出会提示具体的命令，例如：
    echo <latest_commit_hash> > scripts/.httplib_last_sync
    ```

┏━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ Line ┃ Message                                                      ┃ Code                         ┃
┡━━━━━━╇━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┩
│ 75   │ Task returned by function call is ignored (missing co_await?)│ svr->listen_async(...)       │
└──────┴──────────────────────────────────────────────────────────────┴──────────────────────────────┘

Found 1 violations!
```
