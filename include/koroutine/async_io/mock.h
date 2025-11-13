#pragma once
#include "koroutine/async_io/io_object.h
#include "koroutine/awaiters/io_awaiter.hpp"
namespace koroutine::async_io {
class MockAsyncIOObject : public AsyncIOObject {
  std::vector<char> data_;

 public:
  explicit MockAsyncIOObject(IOEngine& engine) : AsyncIOObject(engine) {}
  Task<size_t> read(void* buf, size_t size) override {
    size_t to_read = std::min(size, data_.size());
    std::memcpy(buf, data_.data(), to_read);
    data_.erase(data_.begin(), data_.begin() + to_read);
    co_return to_read;
  }

  Task<size_t> write(const void* buf, size_t size) override {
    const char* char_buf = static_cast<const char*>(buf);
    data_.insert(data_.end(), char_buf, char_buf + size);
    co_return size;
  }

  Task<void> close() override {
    data_.clear();
    co_return;
  }

  Task<void> set_delay(int milliseconds) {
    // 模拟设置延迟操作
    co_return;
  }

  intptr_t native_handle() const override { return 0; }
};
}  // namespace koroutine::async_io