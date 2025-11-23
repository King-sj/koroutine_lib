#pragma once

#include <memory>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <cctype>
#include <concepts>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>

#include "koroutine/async_io/engin.h"
#include "koroutine/async_io/io_object.h"
#include "koroutine/async_io/op.h"
#include "koroutine/awaiters/io_awaiter.hpp"
#include "koroutine/channel.hpp"
#include "koroutine/task.hpp"

namespace koroutine::async_io {

class AsyncStandardStream
    : public AsyncIOObject,
      public std::enable_shared_from_this<AsyncStandardStream> {
 public:
  AsyncStandardStream(std::shared_ptr<IOEngine> engine, int fd)
      : AsyncIOObject(std::move(engine)), fd_(fd) {
#ifndef _WIN32
    // 设置非阻塞模式
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags != -1) {
      ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    }
#endif
  }

  ~AsyncStandardStream() {
    if (output_channel_) {
      output_channel_->close();
    }
  }

  Task<size_t> read(void* buf, size_t size) override {
    auto io_op = std::make_shared<AsyncIOOp>(OpType::READ, shared_from_this(),
                                             buf, size);
    co_return co_await IOAwaiter<size_t>{io_op};
  }

  Task<size_t> write(const void* buf, size_t size) override {
    if (fd_ == 1 || fd_ == 2) {
      if (!output_channel_) {
        std::lock_guard<std::mutex> lock(init_mutex_);
        if (!output_channel_) {
          output_channel_ = std::make_shared<Channel<std::string>>(1024);
          printer_task_ = std::make_shared<Task<void>>(run_printer(
              std::weak_ptr<AsyncStandardStream>(shared_from_this()),
              output_channel_));
          printer_task_->start();
        }
      }
      std::string data((const char*)buf, size);
      co_await output_channel_->write(std::move(data));
      co_return size;
    }

    auto io_op = std::make_shared<AsyncIOOp>(OpType::WRITE, shared_from_this(),
                                             const_cast<void*>(buf), size);
    co_return co_await IOAwaiter<size_t>{io_op};
  }

  Task<void> close() override {
    // 标准流不关闭底层 fd
    if (output_channel_) {
      output_channel_->close();
    }
    co_return;
  }

  intptr_t native_handle() const override { return fd_; }

 private:
  static Task<void> run_printer(std::weak_ptr<AsyncStandardStream> weak_self,
                                std::shared_ptr<Channel<std::string>> channel) {
    try {
      while (true) {
        std::string data = co_await channel->read();
        auto self = weak_self.lock();
        if (!self) break;

        auto io_op = std::make_shared<AsyncIOOp>(
            OpType::WRITE, self, (void*)data.data(), data.size());
        co_await IOAwaiter<size_t>{io_op};
      }
    } catch (...) {
      // Channel closed
    }
  }

  int fd_;
  std::shared_ptr<Channel<std::string>> output_channel_;
  std::shared_ptr<Task<void>> printer_task_;
  std::mutex init_mutex_;
};

class StandardStream {
 public:
  enum Type { Input, Output, Error };

  StandardStream() : type_(Output) {}
  StandardStream(Type type) : type_(type) {}

  Task<size_t> read(void* buf, size_t size) {
    co_return co_await get_impl()->read(buf, size);
  }

  Task<size_t> write(const void* buf, size_t size) {
    co_return co_await get_impl()->write(buf, size);
  }

 private:
  std::shared_ptr<AsyncStandardStream> get_impl() {
    if (!impl_) {
      auto engine = get_default_io_engine();
      int fd = 0;
      switch (type_) {
        case Input:
#ifdef _WIN32
          fd = 0;
#else
          fd = STDIN_FILENO;
#endif
          break;
        case Output:
#ifdef _WIN32
          fd = 1;
#else
          fd = STDOUT_FILENO;
#endif
          break;
        case Error:
#ifdef _WIN32
          fd = 2;
#else
          fd = STDERR_FILENO;
#endif
          break;
      }
      impl_ = std::make_shared<AsyncStandardStream>(engine, fd);
    }
    return impl_;
  }

  Type type_;
  std::shared_ptr<AsyncStandardStream> impl_;
};

template <typename T>
concept Streamable = std::convertible_to<T, std::string_view> ||
                     std::integral<T> || std::floating_point<T>;

template <Streamable T>
Task<StandardStream> operator<<(StandardStream& s, T value) {
  if constexpr (std::convertible_to<T, std::string_view>) {
    std::string_view sv = value;
    co_await s.write(sv.data(), sv.size());
  } else {
    std::string str = std::to_string(value);
    co_await s.write(str.data(), str.size());
  }
  co_return s;
}

template <Streamable T>
Task<StandardStream> operator<<(Task<StandardStream> t, T value) {
  StandardStream s = co_await std::move(t);
  co_await (s << value);
  co_return s;
}

inline Task<StandardStream> operator>>(StandardStream& s, std::string& value) {
  value.clear();
  char c;
  bool skip_ws = true;
  while (true) {
    size_t n = co_await s.read(&c, 1);
    if (n == 0) break;
    if (std::isspace(c)) {
      if (skip_ws)
        continue;
      else
        break;
    }
    skip_ws = false;
    value.push_back(c);
  }
  co_return s;
}

inline Task<StandardStream> operator>>(Task<StandardStream> t,
                                       std::string& value) {
  StandardStream s = co_await std::move(t);
  co_await (s >> value);
  co_return s;
}

inline StandardStream cin(StandardStream::Input);
inline StandardStream cout(StandardStream::Output);
inline StandardStream cerr(StandardStream::Error);

}  // namespace koroutine::async_io
