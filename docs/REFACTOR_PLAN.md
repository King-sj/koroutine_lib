# 协程库重构方案

## 问题总结

### 1. Executor 设计问题
- ❌ **当前**：`Executor executor;` 值成员，每个协程独立实例
- ✅ **应该**：`Executor* _executor;` 指针成员，多协程共享

### 2. 编译期 vs 运行时绑定
- ❌ **当前**：`Task<int, AsyncExecutor>` 编译期绑定
- ✅ **应该**：`Task<int>` + `task.via(executor)` 运行时绑定

### 3. Scheduler 职责混乱
- ❌ **当前**：Scheduler 独立存在，只用于 sleep
- ✅ **应该**：Executor 内置延迟调度能力

### 4. 缺少灵活的 executor 传递机制
- ❌ **当前**：无法在协程链中切换 executor
- ✅ **应该**：支持 `co_await task.via(other_executor)`

## 重构步骤

### 阶段 1：Executor 接口增强

```cpp
class Executor {
public:
  virtual ~Executor() = default;

  // 立即调度
  virtual bool schedule(Func func) = 0;

  // 延迟调度（整合 Scheduler 功能）
  virtual void schedule(Func func, std::chrono::milliseconds delay) {
    // 默认实现：启动线程延迟后调度
    std::thread([this, func = std::move(func), delay]() {
      std::this_thread::sleep_for(delay);
      schedule(std::move(func));
    }).detach();
  }

  // 优先级调度（可选）
  virtual bool schedule(Func func, Priority priority) {
    return schedule(std::move(func));
  }
};
```

### 阶段 2：Task/TaskPromise 重构

```cpp
// 移除 Executor 模板参数
template <typename ResultType>
class Task {
public:
  using promise_type = TaskPromise<ResultType>;

  // 添加 via() 方法
  Task& via(Executor* executor) {
    handle_.promise().set_executor(executor);
    return *this;
  }

private:
  std::coroutine_handle<promise_type> handle_;
};

template <typename ResultType>
struct TaskPromise {
  // Executor 改为指针
  Executor* _executor = nullptr;

  void set_executor(Executor* executor) {
    _executor = executor;
  }

  auto initial_suspend() {
    if (_executor) {
      return DispatchAwaiter{_executor};
    }
    return std::suspend_never{};  // 同步执行
  }

  // ... rest
};
```

### 阶段 3：全局 Executor 管理

```cpp
class ExecutorManager {
public:
  static ExecutorManager& instance() {
    static ExecutorManager mgr;
    return mgr;
  }

  // 设置默认 executor
  void set_default(Executor* executor) {
    default_executor_ = executor;
  }

  Executor* get_default() const {
    return default_executor_;
  }

  // 线程局部 executor（可选）
  void set_thread_local(Executor* executor) {
    thread_local_executor_ = executor;
  }

  Executor* get_current() const {
    return thread_local_executor_ ? thread_local_executor_ : default_executor_;
  }

private:
  Executor* default_executor_ = nullptr;
  static thread_local Executor* thread_local_executor_;
};
```

### 阶段 4：使用示例

```cpp
// 创建共享的 executor
auto thread_pool = std::make_shared<ThreadPoolExecutor>(4);
auto io_executor = std::make_shared<IOExecutor>();

// 设置默认 executor
ExecutorManager::instance().set_default(thread_pool.get());

// 使用
Task<int> compute_task() {
  co_await std::chrono::milliseconds(100);  // 使用默认 executor
  co_return 42;
}

Task<std::string> io_task() {
  // 切换到 IO executor
  co_await switch_executor(io_executor.get());

  auto data = co_await read_file("data.txt");

  // 切换回 thread pool 做计算
  co_await switch_executor(thread_pool.get());

  auto result = process(data);
  co_return result;
}

// 多任务并发（真正共享线程池）
Task<void> main_task() {
  auto t1 = compute_task().via(thread_pool.get());
  auto t2 = compute_task().via(thread_pool.get());
  auto t3 = io_task().via(io_executor.get());

  // 所有任务共享各自的 executor
  co_await when_all(std::move(t1), std::move(t2), std::move(t3));
}
```

## 优势对比

### 当前实现
```cpp
Task<int, AsyncExecutor> task1() { co_return 1; }  // 独立 executor A
Task<int, AsyncExecutor> task2() { co_return 2; }  // 独立 executor B
```
- ❌ 无法共享资源
- ❌ 编译期绑定，不灵活
- ❌ 每个协程一个 executor 实例

### 重构后
```cpp
Task<int> task1() { co_return 1; }
Task<int> task2() { co_return 2; }

auto executor = get_thread_pool();
auto t1 = task1().via(executor);
auto t2 = task2().via(executor);  // 共享同一个 executor
```
- ✅ 共享线程池等资源
- ✅ 运行时灵活绑定
- ✅ 高效的资源利用

## 兼容性考虑

如果需要保持向后兼容，可以保留模板版本作为语法糖：

```cpp
template <typename ResultType, typename ExecutorType>
using TaskWithExecutor = Task<ResultType>;  // 使用 using 别名

// 或提供辅助函数
template <typename ExecutorType, typename Func>
auto make_task(Func&& func, ExecutorType* executor) {
  static ExecutorType static_executor;  // 静态实例
  return func().via(&static_executor);
}
```

## 实施优先级

1. **高优先级**：修复 Executor 指针问题（阶段 1-2）
2. **中优先级**：添加 via() 支持（阶段 2）
3. **低优先级**：全局 executor 管理（阶段 3）

## 参考实现

- **async_simple**：Lazy<T> + via()
- **cppcoro**：task<T> + schedule_on()
- **folly**：Task<T> + via()
- **libunifex**：运行时 scheduler 绑定
