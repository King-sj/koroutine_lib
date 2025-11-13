#pragma once

#include <list>
#include <mutex>
#include <thread>

#include "../awaiters/awaiter.hpp"
#include "async_mutex.h"

namespace koroutine {

class AsyncConditionVariable {
 public:
  AsyncConditionVariable() = default;

  struct WaitAwaiter : public AwaiterBase<void> {
    WaitAwaiter(AsyncConditionVariable* cv, AsyncMutex* m)
        : cv(cv), mutex(m), owns_mutex_on_resume(false) {}
    friend class AsyncConditionVariable;
    // movable, non-copyable
    WaitAwaiter(WaitAwaiter&&) noexcept = default;
    WaitAwaiter& operator=(WaitAwaiter&&) noexcept = default;
    WaitAwaiter(const WaitAwaiter&) = delete;
    WaitAwaiter& operator=(const WaitAwaiter&) = delete;

    void after_suspend() override {
      // release the mutex and enqueue ourselves
      mutex->unlock();
      std::lock_guard<std::mutex> lk(cv->internal_mutex);
      cv->waiters.push_back(this);
    }

    void before_resume() override {
      // If notify transferred ownership, mutex is already held for us.
      // Otherwise, we need to acquire mutex before resuming; try to acquire
      // now.
      if (!owns_mutex_on_resume) {
        // blocking re-acquire: since AsyncMutex is async, we try to take it
        // directly. To avoid complex nested awaiting inside before_resume,
        // perform busy-spin acquire. This is a simple approach: spin until lock
        // is acquired. Expect notifications to be quick.
        while (!mutex->try_lock()) {
          // small yield - not ideal but keeps implementation header-only and
          // simple
          std::this_thread::yield();
        }
      }
    }

    ~WaitAwaiter() {
      // remove from cv waiters if still present
      if (cv) {
        std::lock_guard<std::mutex> lk(cv->internal_mutex);
        cv->waiters.remove(this);
      }
    }

    AsyncConditionVariable* cv;
    AsyncMutex* mutex;
    bool owns_mutex_on_resume;
  };

  WaitAwaiter wait(AsyncMutex& m) { return WaitAwaiter(this, &m); }

  void notify_one() {
    WaitAwaiter* w = nullptr;
    {
      std::lock_guard<std::mutex> lk(internal_mutex);
      if (!waiters.empty()) {
        w = waiters.front();
        waiters.pop_front();
      }
    }
    if (w) {
      // transfer ownership of mutex to waiter: set mutex locked and mark waiter
      w->mutex->acquire_for_notify();
      w->owns_mutex_on_resume = true;
      w->resume();
    }
  }

  void notify_all() {
    std::list<WaitAwaiter*> list_to_wake;
    {
      std::lock_guard<std::mutex> lk(internal_mutex);
      list_to_wake.swap(waiters);
    }
    for (auto w : list_to_wake) {
      w->mutex->acquire_for_notify();
      w->owns_mutex_on_resume = true;
      w->resume();
    }
  }

 private:
  std::mutex internal_mutex;
  std::list<WaitAwaiter*> waiters;
};

}  // namespace koroutine
