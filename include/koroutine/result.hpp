#pragma once
#include <exception>

#include "koroutine/debug.h"
namespace koroutine {
// define unintialized exception
class UninitializedResultException : public std::exception {
 public:
  const char* what() const noexcept override {
    return "Result is uninitialized";
  }
};

// CRTP 基类
template <typename T, typename Derived>
struct ResultBase {
  explicit ResultBase() = default;

  explicit ResultBase(std::exception_ptr&& exception_ptr)
      : _exception_ptr(std::move(exception_ptr)) {}

 protected:
  std::exception_ptr _exception_ptr;
};

// 通用模板 - 非 void 类型
template <typename T>
struct Result : ResultBase<T, Result<T>> {
  using Base = ResultBase<T, Result<T>>;

  explicit Result() = default;

  explicit Result(T&& value) : _value(std::move(value)) {
    LOG_TRACE("Result<T>::ctor from T&& - constructing with value");
  }

  explicit Result(std::exception_ptr&& exception_ptr)
      : Base(std::move(exception_ptr)) {}

  // 移动构造函数
  Result(Result&& other) noexcept
      : Base(std::move(other._exception_ptr)), _value(std::move(other._value)) {
    LOG_TRACE("Result<T>::move ctor - moving from other Result");
  }

  // 移动赋值运算符
  Result& operator=(Result&& other) noexcept {
    LOG_TRACE("Result<T>::move assignment - assigning from other Result");
    if (this != &other) {
      this->_exception_ptr = std::move(other._exception_ptr);
      _value = std::move(other._value);
    }
    return *this;
  }

  // 禁用拷贝
  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

  bool has_exception() const { return static_cast<bool>(this->_exception_ptr); }

  void rethrow_exception() {
    if (this->_exception_ptr) {
      std::rethrow_exception(this->_exception_ptr);
    }
  }

  T get_or_throw() {
    LOG_TRACE("Result::get_or_throw - checking for exception");
    if (this->_exception_ptr) {
      LOG_ERROR("Result::get_or_throw - throwing stored exception");
      std::rethrow_exception(this->_exception_ptr);
    }
    LOG_TRACE("Result::get_or_throw - returning std::move(_value)");
    return std::move(_value);
  }

 private:
  T _value{};
};

// void 类型的特化
template <>
struct Result<void> : ResultBase<void, Result<void>> {
  using Base = ResultBase<void, Result<void>>;

  explicit Result() : Base() {
    // void 类型默认已初始化，清除异常
    _exception_ptr = nullptr;
  }

  explicit Result(std::exception_ptr&& exception_ptr)
      : Base(std::move(exception_ptr)) {}

  bool has_exception() const { return static_cast<bool>(_exception_ptr); }

  void rethrow_exception() {
    if (_exception_ptr) {
      std::rethrow_exception(_exception_ptr);
    }
  }

  void get_or_throw() {
    LOG_TRACE("Result<void>::get_or_throw - checking for exception");
    if (_exception_ptr) {
      LOG_ERROR("Result<void>::get_or_throw - throwing stored exception");
      std::rethrow_exception(_exception_ptr);
    }
  }
};

}  // namespace koroutine