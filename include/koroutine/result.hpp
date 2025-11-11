#pragma once
#include <exception>

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
      : _exception_ptr(exception_ptr) {}

 protected:
  std::exception_ptr _exception_ptr =
      std::make_exception_ptr(UninitializedResultException());
};

// 通用模板 - 非 void 类型
template <typename T>
struct Result : ResultBase<T, Result<T>> {
  using Base = ResultBase<T, Result<T>>;

  explicit Result() = default;

  explicit Result(T&& value) : _value(value) {}

  explicit Result(std::exception_ptr&& exception_ptr)
      : Base(std::move(exception_ptr)) {}

  T get_or_throw() {
    LOG_TRACE("Result::get_or_throw - checking for exception with value",
              _value);
    if (this->_exception_ptr) {
      LOG_ERROR("Result::get_or_throw - throwing stored exception");
      std::rethrow_exception(this->_exception_ptr);
    }
    return _value;
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

  void get_or_throw() {
    LOG_TRACE("Result<void>::get_or_throw - checking for exception");
    if (_exception_ptr) {
      LOG_ERROR("Result<void>::get_or_throw - throwing stored exception");
      std::rethrow_exception(_exception_ptr);
    }
  }
};

}  // namespace koroutine