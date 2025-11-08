#pragma once
#include <exception>
#include <list>

#include "coroutine_common.h"
namespace koroutine {

template <typename T>
struct Generator {
  class ExhaustedException : std::exception {};

  struct promise_type {
    T value;
    bool is_ready = false;

    std::suspend_never initial_suspend() { return {}; };

    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T value) {
      this->value = value;
      is_ready = true;
      return {};
    }

    void unhandled_exception() {}

    Generator get_return_object() {
      return Generator{
          std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    void return_void() {}
  };

  std::coroutine_handle<promise_type> handle;

  bool has_next() {
    if (!handle || handle.done()) {
      return false;
    }

    if (!handle.promise().is_ready) {
      handle.resume();
    }

    if (handle.done()) {
      return false;
    } else {
      return true;
    }
  }

  T next() {
    if (has_next()) {
      handle.promise().is_ready = false;
      return handle.promise().value;
    }
    throw ExhaustedException();
  }

  template <typename F>
  Generator<std::invoke_result_t<F, T>> map(F f) {
    auto up_steam = std::move(*this);
    while (up_steam.has_next()) {
      co_yield f(up_steam.next());
    }
  }

  template <typename F>
  std::invoke_result_t<F, T> flat_map(F f) {
    auto up_steam = std::move(*this);
    while (up_steam.has_next()) {
      auto generator = f(up_steam.next());
      while (generator.has_next()) {
        co_yield generator.next();
      }
    }
  }

  Generator take(int n) {
    auto up_steam = std::move(*this);
    int i = 0;
    while (i++ < n && up_steam.has_next()) {
      co_yield up_steam.next();
    }
  }

  template <typename F>
  Generator take_while(F f) {
    auto up_steam = std::move(*this);
    while (up_steam.has_next()) {
      T value = up_steam.next();
      if (f(value)) {
        co_yield value;
      } else {
        break;
      }
    }
  }

  template <typename F>
  Generator filter(F f) {
    auto up_steam = std::move(*this);
    while (up_steam.has_next()) {
      T value = up_steam.next();
      if (f(value)) {
        co_yield value;
      }
    }
  }

  template <typename F>
  void for_each(F f) {
    while (has_next()) {
      f(next());
    }
  }

  template <typename R, typename F>
  R fold(R initial, F f) {
    while (has_next()) {
      initial = f(initial, next());
    }
    return initial;
  }

  T sum() {
    T sum = 0;
    while (has_next()) {
      sum += next();
    }
    return sum;
  }

  Generator static from_array(T array[], int n) {
    for (int i = 0; i < n; ++i) {
      co_yield array[i];
    }
  }

  Generator static from_list(std::list<T> list) {
    for (auto t : list) {
      co_yield t;
    }
  }

  Generator static from(std::initializer_list<T> args) {
    for (auto t : args) {
      co_yield t;
    }
  }

  template <typename... TArgs>
  Generator static from(TArgs... args) {
    (co_yield args, ...);
  }

  explicit Generator(std::coroutine_handle<promise_type> handle) noexcept
      : handle(handle) {}

  Generator(Generator&& generator) noexcept
      : handle(std::exchange(generator.handle, {})) {}

  Generator(Generator&) = delete;
  Generator& operator=(Generator&) = delete;

  ~Generator() {
    if (handle) handle.destroy();
  }
};

}  // namespace koroutine