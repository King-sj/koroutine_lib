#pragma once
#include "../coroutine_common.h"
#include "../result.hpp"
#include "../schedulers/scheduler.h"

namespace koroutine {

// CRTP 基类 - 包含公共的 awaiter 逻辑
template <typename R, typename Derived>
class AwaiterBaseCRTP {
 public:
  using ResultType = R;

  // default constructor
  AwaiterBaseCRTP() = default;

  // move constructor
  AwaiterBaseCRTP(AwaiterBaseCRTP&& awaiter) noexcept
      : _scheduler(std::move(awaiter._scheduler)),
        _handle(std::move(awaiter._handle)),
        _result(std::move(awaiter._result)) {
    LOG_INFO("AwaiterBaseCRTP::move constructor - moved awaiter");
  }

  virtual bool await_ready() const {
    LOG_TRACE("AwaiterBase::await_ready - default implementation always false");
    return false;
  }

  void await_suspend(std::coroutine_handle<> handle) {
    LOG_INFO(
        "AwaiterBase::await_suspend - suspending coroutine, and storing "
        "the caller handle  ",
        handle.address());
    this->_handle = handle;
    static_cast<Derived*>(this)->after_suspend();
  }

  void install_scheduler(std::shared_ptr<AbstractScheduler> scheduler) {
    if (!scheduler) {
      LOG_ERROR("AwaiterBase::install_scheduler - null scheduler provided");
    }
    LOG_TRACE("AwaiterBase::install_scheduler - installing scheduler");
    _scheduler = scheduler;
  }

 protected:
  void dispatch(std::function<void()>&& f) {
    if (!_handle) {
      LOG_ERROR(
          "AwaiterBase::dispatch - no coroutine handle set, cannot dispatch.");
      return;
    }
    if (_scheduler) {
      _scheduler->schedule(std::move(f), 0);
    } else {
      LOG_ERROR(
          "AwaiterBase::dispatch - no scheduler bound, executing on current "
          "thread.");
      f();
    }
  }
  void resume_unsafe() {
    dispatch([this]() { _handle.resume(); });
  }

 protected:
  std::optional<Result<R>> _result{};
  std::shared_ptr<AbstractScheduler> _scheduler = nullptr;
  //   存储调用者的协程句柄，用于awaiter执行结束后恢复调用者协程
  std::coroutine_handle<> _handle = nullptr;
};

// 通用模板 - 非 void 类型
template <typename R>
class AwaiterBase : public AwaiterBaseCRTP<R, AwaiterBase<R>> {
  friend class AwaiterBaseCRTP<R, AwaiterBase<R>>;

 public:
  using Base = AwaiterBaseCRTP<R, AwaiterBase<R>>;
  using ResultType = R;
  using Base::_handle;
  using Base::_result;
  using Base::_scheduler;
  using Base::dispatch;

  // default constructor
  AwaiterBase() = default;

  // move constructor
  AwaiterBase(AwaiterBase&& awaiter) noexcept : Base(std::move(awaiter)) {}

  R await_resume() {
    LOG_TRACE("AwaiterBase::await_resume - preparing to resume");
    before_resume();
    LOG_TRACE("AwaiterBase::await_resume - returning result");
    return _result->get_or_throw();
  }

 protected:
  void resume(R value) {
    dispatch([this, value]() {
      _result = Result<R>(static_cast<R>(value));
      _handle.resume();
    });
  }

  void resume_exception(std::exception_ptr&& e) {
    dispatch([this, e]() {
      _result = Result<R>(static_cast<std::exception_ptr>(e));
      _handle.resume();
    });
  }
  virtual void after_suspend() {
    LOG_INFO(
        "AwaiterBase::after_suspend - default implementation does nothing.");
  }
  virtual void before_resume() {
    LOG_INFO(
        "AwaiterBase::before_resume - default implementation does nothing.");
  }
};

// void 类型的特化
template <>
class AwaiterBase<void> : public AwaiterBaseCRTP<void, AwaiterBase<void>> {
  friend class AwaiterBaseCRTP<void, AwaiterBase<void>>;

 public:
  using Base = AwaiterBaseCRTP<void, AwaiterBase<void>>;
  using ResultType = void;
  using Base::_handle;
  using Base::_result;
  using Base::_scheduler;
  using Base::dispatch;

  // default constructor
  AwaiterBase() = default;

  // move constructor
  AwaiterBase(AwaiterBase&& awaiter) noexcept : Base(std::move(awaiter)) {}

  void await_resume() {
    LOG_TRACE("AwaiterBase<void>::await_resume - preparing to resume");
    before_resume();
    LOG_TRACE("AwaiterBase<void>::await_resume - returning result : void");
    _result->get_or_throw();
  }

 protected:
  void resume() {
    dispatch([this]() {
      _result = Result<void>();
      _handle.resume();
    });
  }

  void resume_exception(std::exception_ptr&& e) {
    dispatch([this, e]() {
      _result = Result<void>(static_cast<std::exception_ptr>(e));
      _handle.resume();
    });
  }
  virtual void after_suspend() {
    LOG_INFO(
        "AwaiterBase::after_suspend - default implementation does nothing.");
  }
  virtual void before_resume() {
    LOG_INFO(
        "AwaiterBase::before_resume - default implementation does nothing.");
  }
};

}  // namespace koroutine
