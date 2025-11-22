#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "debug.h"

namespace koroutine {

/**
 * @brief 操作取消异常
 *
 * 当协程任务被取消时抛出此异常。
 */
class OperationCancelledException : public std::exception {
 public:
  const char* what() const noexcept override {
    return "Operation was cancelled";
  }
};

/**
 * @brief 取消令牌
 *
 * 用于协作式取消异步操作。多个操作可以共享同一个取消令牌，
 * 当调用 cancel() 时，所有监听此令牌的操作都会收到取消通知。
 *
 * 使用示例:
 * @code
 * CancellationToken token;
 *
 * auto task = [](CancellationToken token) -> Task<void> {
 *   for (int i = 0; i < 100; ++i) {
 *     if (token.is_cancelled()) {
 *       LOG_INFO("Task cancelled at iteration ", i);
 *       co_return;
 *     }
 *     co_await do_work();
 *   }
 * };
 *
 * auto t = task(token);
 * t.start();
 *
 * // 在某个时刻取消
 * token.cancel();
 * @endcode
 */
class CancellationToken {
 public:
  /**
   * @brief 构造一个新的取消令牌
   */
  CancellationToken() : state_(std::make_shared<State>()) {
    LOG_TRACE("CancellationToken::constructor - created new token");
  }

  /**
   * @brief 检查是否已被取消
   * @return true 如果已取消，false 否则
   */
  bool is_cancelled() const {
    return state_->cancelled.load(std::memory_order_acquire);
  }

  /**
   * @brief 注册取消回调
   *
   * 当令牌被取消时，回调将被调用。如果令牌已经被取消，
   * 回调将立即被调用。
   *
   * @param callback 取消时要调用的函数
   *
   * 注意：回调可能在任意线程上执行
   */
  void on_cancel(std::function<void()> callback) {
    std::lock_guard lock(state_->mtx);

    if (state_->cancelled.load(std::memory_order_acquire)) {
      // 已经取消，立即调用回调
      LOG_TRACE(
          "CancellationToken::on_cancel - already cancelled, invoking callback "
          "immediately");
      callback();
    } else {
      // 还未取消，注册回调
      LOG_TRACE("CancellationToken::on_cancel - registering callback");
      state_->callbacks.push_back(std::move(callback));
    }
  }

  /**
   * @brief 请求取消
   *
   * 将令牌标记为已取消，并调用所有注册的回调。
   * 此方法是线程安全的，可以从任意线程调用。
   * 多次调用是安全的（后续调用无效果）。
   */
  void cancel() {
    std::lock_guard lock(state_->mtx);

    // 使用 exchange 确保只执行一次
    bool was_cancelled =
        state_->cancelled.exchange(true, std::memory_order_acq_rel);

    if (!was_cancelled) {
      LOG_INFO("CancellationToken::cancel - cancelling, invoking ",
               state_->callbacks.size(), " callbacks");

      // 调用所有回调
      for (auto& cb : state_->callbacks) {
        try {
          cb();
        } catch (const std::exception& e) {
          LOG_ERROR("CancellationToken::cancel - exception in callback: ",
                    e.what());
        } catch (...) {
          LOG_ERROR(
              "CancellationToken::cancel - unknown exception in callback");
        }
      }

      // 清空回调列表
      state_->callbacks.clear();
    } else {
      LOG_TRACE("CancellationToken::cancel - already cancelled, ignoring");
    }
  }

  /**
   * @brief 如果已取消则抛出异常
   *
   * 便捷方法，用于在协程中检查取消状态。
   *
   * @throws OperationCancelledException 如果令牌已被取消
   *
   * 使用示例:
   * @code
   * Task<void> my_task(CancellationToken token) {
   *   for (int i = 0; i < 100; ++i) {
   *     token.throw_if_cancelled();  // 如果取消则抛异常
   *     co_await do_work();
   *   }
   * }
   * @endcode
   */
  void throw_if_cancelled() const {
    if (is_cancelled()) {
      LOG_TRACE(
          "CancellationToken::throw_if_cancelled - throwing "
          "OperationCancelledException");
      throw OperationCancelledException();
    }
  }

  /**
   * @brief 重置令牌（用于测试）
   *
   * 注意：此方法主要用于测试。在生产代码中重用取消令牌可能导致
   * 难以调试的问题。建议每次操作使用新的令牌。
   */
  void reset() {
    std::lock_guard lock(state_->mtx);
    state_->cancelled.store(false, std::memory_order_release);
    state_->callbacks.clear();
    LOG_TRACE("CancellationToken::reset - token reset");
  }

 private:
  struct State {
    std::atomic<bool> cancelled{false};
    std::mutex mtx;
    std::vector<std::function<void()>> callbacks;
  };

  std::shared_ptr<State> state_;
};

/**
 * @brief 取消令牌源
 *
 * 管理取消令牌的生命周期。提供一个令牌供操作使用，
 * 并提供取消该令牌的方法。
 *
 * 使用示例:
 * @code
 * CancellationTokenSource source;
 *
 * auto task = long_running_operation(source.token());
 * task.start();
 *
 * // 5秒后取消
 * std::this_thread::sleep_for(5s);
 * source.cancel();
 * @endcode
 */
class CancellationTokenSource {
 public:
  /**
   * @brief 构造一个新的取消令牌源
   */
  CancellationTokenSource() : token_() {
    LOG_TRACE("CancellationTokenSource::constructor");
  }

  /**
   * @brief 获取关联的取消令牌
   */
  CancellationToken token() const { return token_; }

  /**
   * @brief 请求取消
   */
  void cancel() {
    LOG_TRACE("CancellationTokenSource::cancel - cancelling token");
    token_.cancel();
  }

  /**
   * @brief 检查是否已取消
   */
  bool is_cancelled() const { return token_.is_cancelled(); }

 private:
  CancellationToken token_;
};

}  // namespace koroutine
