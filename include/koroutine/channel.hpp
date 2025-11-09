#pragma once

#include <exception>
#include <list>
#include <queue>

#include "awaiters/channel_awaiter.hpp"
#include "coroutine_common.h"
#include "task.hpp"
namespace koroutine {
template <typename ValueType>
struct Channel {
  struct ChannelClosedException : std::exception {
    const char* what() const noexcept override { return "Channel is closed."; }
  };

  void check_closed() {
    if (!_is_active.load(std::memory_order_relaxed)) {
      throw ChannelClosedException();
    }
  }

  void try_push_reader(ReaderAwaiter<ValueType>* reader_awaiter) {
    std::unique_lock lock(channel_lock);
    check_closed();

    if (!buffer.empty()) {
      auto value = buffer.front();
      buffer.pop();

      if (!writer_list.empty()) {
        auto writer = writer_list.front();
        writer_list.pop_front();
        buffer.push(writer->_value);
        lock.unlock();

        writer->resume();
      } else {
        lock.unlock();
      }

      reader_awaiter->resume(value);
      return;
    }

    if (!writer_list.empty()) {
      auto writer = writer_list.front();
      writer_list.pop_front();
      lock.unlock();

      reader_awaiter->resume(writer->_value);
      writer->resume();
      return;
    }

    reader_list.push_back(reader_awaiter);
  }

  void try_push_writer(WriterAwaiter<ValueType>* writer_awaiter) {
    LOG_TRACE("Channel::try_push_writer - trying to push writer");
    std::unique_lock lock(channel_lock);
    LOG_TRACE("Channel::try_push_writer - acquired lock");
    check_closed();
    // suspended readers
    if (!reader_list.empty()) {
      auto reader = reader_list.front();
      reader_list.pop_front();
      lock.unlock();

      reader->resume(writer_awaiter->_value);
      writer_awaiter->resume();
      return;
    }

    // write to buffer
    if (buffer.size() < buffer_capacity) {
      buffer.push(writer_awaiter->_value);
      lock.unlock();
      writer_awaiter->resume();
      return;
    }

    // suspend writer
    writer_list.push_back(writer_awaiter);
    LOG_TRACE("Channel::try_push_writer - suspending writer");
  }

  void remove_writer(WriterAwaiter<ValueType>* writer_awaiter) {
    std::lock_guard lock(channel_lock);
    auto size = writer_list.remove(writer_awaiter);
    LOG_DEBUG("remove writer ", size);
  }

  void remove_reader(ReaderAwaiter<ValueType>* reader_awaiter) {
    std::lock_guard lock(channel_lock);
    auto size = reader_list.remove(reader_awaiter);
    LOG_DEBUG("remove reader ", size);
  }

  auto write(ValueType value) {
    check_closed();
    return WriterAwaiter<ValueType>{this, value};
  }

  auto operator<<(ValueType value) { return write(value); }

  auto read() {
    check_closed();
    return ReaderAwaiter<ValueType>{this};
  }

  auto operator>>(ValueType& value_ref) {
    auto awaiter = read();
    awaiter.p_value = &value_ref;
    return awaiter;
  }

  void close() {
    bool expected = true;
    if (_is_active.compare_exchange_strong(expected, false,
                                           std::memory_order_relaxed)) {
      clean_up();
    }
  }
  /**
   * Close the channel when it becomes empty, checking every `check_interval_ms`
   * milliseconds. If `timeout` is non-negative, gives up after that many
   * milliseconds.
   *
   * Returns 0 if closed successfully, -1 if timed out.
   */
  Task<int> close_when_empty(long long timeout = -1,
                             long long check_interval_ms = 100) {
    co_await koroutine::SleepAwaiter(
        0);  // yield to allow other coroutines to run
    auto end_time = (timeout >= 0)
                        ? (std::chrono::steady_clock::now() +
                           std::chrono::milliseconds(timeout))
                        : std::chrono::steady_clock::time_point::max();
    while (timeout < 0 || std::chrono::steady_clock::now() < end_time) {
      LOG_TRACE("Channel::close_when_empty - checking buffer");
      {
        std::lock_guard lock(channel_lock);
        if (buffer.empty()) {
          break;
        }
      }
      co_await koroutine::SleepAwaiter(
          check_interval_ms);  // avoid busy waiting
    }
    if (timeout != -1 && std::chrono::steady_clock::now() >= end_time) {
      LOG_WARN("Channel::close_when_empty - timeout reached, not closing");
      co_return -1;
    }
    close();
    co_return 0;
  }

  explicit Channel(int capacity = 0) : buffer_capacity(capacity) {
    _is_active.store(true, std::memory_order_relaxed);
  }

  bool is_active() const { return _is_active.load(std::memory_order_relaxed); }

  Channel(const Channel&&) = delete;
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  ~Channel() {
    LOG_TRACE("Channel::~Channel - closing channel");
    close();
  }

 private:
  int buffer_capacity;
  std::queue<ValueType> buffer;
  std::list<WriterAwaiter<ValueType>*> writer_list;
  std::list<ReaderAwaiter<ValueType>*> reader_list;

  std::atomic<bool> _is_active;

  std::mutex channel_lock;
  std::condition_variable channel_condition;

  void clean_up() {
    std::lock_guard lock(channel_lock);

    for (auto writer : writer_list) {
      writer->resume();
    }
    LOG_TRACE("Channel::clean_up - resuming ", writer_list.size(), " writers");
    writer_list.clear();

    for (auto reader : reader_list) {
      reader->resume_unsafe();
    }
    LOG_TRACE("Channel::clean_up - resuming ", reader_list.size(), " readers");
    reader_list.clear();

    decltype(buffer) empty_buffer;
    std::swap(buffer, empty_buffer);
    LOG_TRACE("Channel::clean_up - cleared buffer");
  }
};
}  // namespace koroutine