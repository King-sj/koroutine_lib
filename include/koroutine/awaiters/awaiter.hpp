#pragma once
#include "../coroutine_common.h"
#include "../executors/executor.h"
#include "../result.hpp"

namespace koroutine {

template <typename R>
class AwaiterBase {
 public:
  using ResultType = R;

  bool await_ready() const { return false; }

  void await_suspend(std::coroutine_handle<> handle) {
    this->_handle = handle;
    after_suspend();
  }

  R await_resume() {
    before_resume();
    return _result->get_or_throw();
  }

  void install_executor(std::shared_ptr<AbstractExecutor> executor) {
    _executor = executor;
  }

 protected:
  std::optional<Result<R>> _result{};

  void resume(R value) {
    dispatch([this, value]() {
      _result = Result<R>(static_cast<R>(value));
      _handle.resume();
    });
  }

  void resume_unsafe() {
    dispatch([this]() { _handle.resume(); });
  }

  void resume_exception(std::exception_ptr&& e) {
    dispatch([this, e]() {
      _result = Result<R>(static_cast<std::exception_ptr>(e));
      _handle.resume();
    });
  }

  virtual void after_suspend() {}

  virtual void before_resume() {}

 private:
  std::shared_ptr<AbstractExecutor> _executor = nullptr;
  std::coroutine_handle<> _handle = nullptr;

  void dispatch(std::function<void()>&& f) {
    if (_executor) {
      _executor->execute(std::move(f));
    } else {
      f();
    }
  }
};

template <>
class AwaiterBase<void> {
 public:
  using ResultType = void;

  bool await_ready() const { return false; }

  void await_suspend(std::coroutine_handle<> handle) {
    this->_handle = handle;
    after_suspend();
  }

  void await_resume() {
    before_resume();
    _result->get_or_throw();
  }

  void install_executor(std::shared_ptr<AbstractExecutor> executor) {
    _executor = executor;
  }

 protected:
  std::optional<Result<void>> _result{};
  std::shared_ptr<AbstractExecutor> _executor = nullptr;

  void resume() {
    dispatch([this]() {
      _result = Result<void>();
      _handle.resume();
    });
  }

  void resume_unsafe() {
    dispatch([this]() { _handle.resume(); });
  }

  void resume_exception(std::exception_ptr&& e) {
    dispatch([this, e]() {
      _result = Result<void>(static_cast<std::exception_ptr>(e));
      _handle.resume();
    });
  }

  virtual void after_suspend() {}

  virtual void before_resume() {}

 private:
  std::coroutine_handle<> _handle = nullptr;

  void dispatch(std::function<void()>&& f) {
    if (_executor) {
      _executor->execute(std::move(f));
    } else {
      f();
    }
  }
};

}  // namespace koroutine
