#pragma once

#include "../coroutine_common.h"
#include "awaiter.hpp"

namespace koroutine {
template <typename ValueType>
struct Channel;

template <typename ValueType>
struct WriterAwaiter : public AwaiterBase<void> {
  friend struct Channel<ValueType>;
  Channel<ValueType>* channel;
  ValueType _value;

  WriterAwaiter(Channel<ValueType>* channel, ValueType value)
      : channel(channel), _value(value) {}

  WriterAwaiter(WriterAwaiter&& other) noexcept
      : AwaiterBase<void>(std::move(other)),
        channel(std::exchange(other.channel, nullptr)),
        _value(std::move(other._value)) {}

  ~WriterAwaiter() {
    if (channel) channel->remove_writer(this);
  }

 protected:
  void after_suspend() override { channel->try_push_writer(this); }

  void before_resume() override {
    channel->check_closed();
    channel = nullptr;
  }
};

template <typename ValueType>
struct ReaderAwaiter : public AwaiterBase<ValueType> {
  friend struct Channel<ValueType>;
  Channel<ValueType>* channel;
  ValueType* p_value = nullptr;

  explicit ReaderAwaiter(Channel<ValueType>* channel)
      : AwaiterBase<ValueType>(), channel(channel) {}
  ReaderAwaiter(ReaderAwaiter&& other) noexcept
      : AwaiterBase<ValueType>(std::move(other)),
        channel(std::exchange(other.channel, nullptr)),
        p_value(std::exchange(other.p_value, nullptr)) {}

  ~ReaderAwaiter() {
    if (channel) channel->remove_reader(this);
  }

 protected:
  void after_suspend() override { channel->try_push_reader(this); }

  void before_resume() override {
    channel->check_closed();
    if (p_value) {
      *p_value = this->_result->get_or_throw();
    }
    channel = nullptr;
  }
};

}  // namespace koroutine
