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
template <typename T>
struct Result {
  explicit Result() = default;

  explicit Result(T&& value) : _value(value) {}

  explicit Result(std::exception_ptr&& exception_ptr)
      : _exception_ptr(exception_ptr) {}

  T get_or_throw() {
    LOG_TRACE("Result::get_or_throw - checking for exception with value",
              _value);
    if (_exception_ptr) {
      LOG_ERROR("Result::get_or_throw - throwing stored exception");
      std::rethrow_exception(_exception_ptr);
    }
    return _value;
  }

 private:
  T _value{};
  //   TODO: 评估对性能的影响
  std::exception_ptr _exception_ptr =
      std::make_exception_ptr(UninitializedResultException());
};

template <>
struct Result<void> {
  explicit Result() = default;

  explicit Result(std::exception_ptr&& exception_ptr)
      : _exception_ptr(exception_ptr) {}

  void get_or_throw() {
    LOG_TRACE("Result<void>::get_or_throw - checking for exception");
    if (_exception_ptr) {
      LOG_ERROR("Result<void>::get_or_throw - throwing stored exception");
      std::rethrow_exception(_exception_ptr);
    }
  }

 private:
  std::exception_ptr _exception_ptr;
};

}  // namespace koroutine