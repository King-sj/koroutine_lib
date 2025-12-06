#pragma once

#include <list>
#include <mutex>
#include <utility>

#include "../awaiters/awaiter.hpp"

namespace koroutine {

class AsyncMutex {
 public:
  AsyncMutex() { _locked = false; }

  struct LockAwaiter : public AwaiterBase<void> {
    explicit LockAwaiter(AsyncMutex* m)
        : mutex(m), owns_mutex_on_resume(false) {}
    friend class AsyncMutex;
    // enable move, disable copy
    LockAwaiter(LockAwaiter&&) noexcept = default;
    LockAwaiter& operator=(LockAwaiter&&) noexcept = default;
    LockAwaiter(const LockAwaiter&) = delete;
    LockAwaiter& operator=(const LockAwaiter&) = delete;

    // When suspended, try to acquire or enqueue
    void after_suspend() override {
      std::unique_lock<std::mutex> lk(mutex->internal_mutex);
      if (!mutex->_locked) {
        // acquire immediately and resume
        mutex->_locked = true;
        owns_mutex_on_resume = true;
        lk.unlock();
        // resume the awaiting coroutine immediately
        this->resume();
        return;
      }
      // otherwise enqueue
      mutex->waiters.push_back(this);
    }

    void before_resume()
        override { /* ownership is indicated by owns_mutex_on_resume */ }

    ~LockAwaiter() {
      // remove from waiters if still enqueued
      if (mutex) {
        std::lock_guard<std::mutex> lk(mutex->internal_mutex);
        mutex->waiters.remove(this);
      }
    }

    bool owns_mutex_on_resume;
    AsyncMutex* mutex;
  };

  // request a lock (co_awaitable)
  LockAwaiter lock() { return LockAwaiter(this); }

  // try to acquire immediately
  bool try_lock() {
    std::lock_guard<std::mutex> lk(internal_mutex);
    if (_locked) return false;
    _locked = true;
    return true;
  }

  // Called by condition variable notify to transfer ownership to a waiter
  void acquire_for_notify() {
    std::lock_guard<std::mutex> lk(internal_mutex);
    _locked = true;
  }

  // unlock and wake next waiter if any
  void unlock() {
    LockAwaiter* next = nullptr;
    {
      std::lock_guard<std::mutex> lk(internal_mutex);
      if (!waiters.empty()) {
        next = waiters.front();
        waiters.pop_front();
      } else {
        _locked = false;
      }
    }
    if (next) {
      // transferring lock ownership to next waiter: mark and resume it
      next->owns_mutex_on_resume = true;
      // keep _locked = true to indicate ownership transferred
      next->resume();
    }
  }

 private:
  std::mutex internal_mutex;
  bool _locked;
  std::list<LockAwaiter*> waiters;
};

}  // namespace koroutine
