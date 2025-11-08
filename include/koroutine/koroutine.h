#pragma once

#include <coroutine>
#include <exception>
#include <iostream>
#include <optional>

namespace koroutine {

/**
 * @brief 简单的Task协程类型
 * @tparam T 协程返回值类型
 */
template <typename T = void>
class Task {
 public:
  struct promise_type {
    std::optional<T> result;
    std::exception_ptr exception;

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_value(T value) { result = std::move(value); }

    void unhandled_exception() { exception = std::current_exception(); }
  };

  using handle_type = std::coroutine_handle<promise_type>;

  Task(handle_type h) : handle_(h) {}

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  Task(Task&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  /**
   * @brief 恢复协程执行
   */
  void resume() {
    if (handle_ && !handle_.done()) {
      handle_.resume();
    }
  }

  /**
   * @brief 检查协程是否完成
   */
  bool done() const { return handle_.done(); }

  /**
   * @brief 获取协程结果
   */
  T get_result() {
    if (!handle_.done()) {
      throw std::runtime_error("Coroutine not finished");
    }
    if (handle_.promise().exception) {
      std::rethrow_exception(handle_.promise().exception);
    }
    if (!handle_.promise().result.has_value()) {
      throw std::runtime_error("No result available");
    }
    return *handle_.promise().result;
  }

 private:
  handle_type handle_;
};

/**
 * @brief Task的void特化版本
 */
template <>
class Task<void> {
 public:
  struct promise_type {
    std::exception_ptr exception;

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_void() {}

    void unhandled_exception() { exception = std::current_exception(); }
  };

  using handle_type = std::coroutine_handle<promise_type>;

  Task(handle_type h) : handle_(h) {}

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  Task(Task&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  void resume() {
    if (handle_ && !handle_.done()) {
      handle_.resume();
    }
  }

  bool done() const { return handle_.done(); }

  void get_result() {
    if (!handle_.done()) {
      throw std::runtime_error("Coroutine not finished");
    }
    if (handle_.promise().exception) {
      std::rethrow_exception(handle_.promise().exception);
    }
  }

 private:
  handle_type handle_;
};

/**
 * @brief 简单的Generator生成器
 * @tparam T 生成的值类型
 */
template <typename T>
class Generator {
 public:
  struct promise_type {
    T current_value;

    Generator get_return_object() {
      return Generator{
          std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T value) {
      current_value = value;
      return {};
    }

    void return_void() {}
    void unhandled_exception() { std::terminate(); }
  };

  using handle_type = std::coroutine_handle<promise_type>;

  Generator(handle_type h) : handle_(h) {}

  Generator(const Generator&) = delete;
  Generator& operator=(const Generator&) = delete;

  Generator(Generator&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  Generator& operator=(Generator&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~Generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

  bool next() {
    if (handle_ && !handle_.done()) {
      handle_.resume();
      return !handle_.done();
    }
    return false;
  }

  T value() const { return handle_.promise().current_value; }

 private:
  handle_type handle_;
};

}  // namespace koroutine
