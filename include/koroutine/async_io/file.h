#pragma once

// 平台相关的头文件
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <ios>
#include <memory>
#include <string>
#include <system_error>

#include "koroutine/async_io/io_object.h"
#include "koroutine/awaiters/io_awaiter.hpp"
#include "koroutine/debug.h"
#include "koroutine/task.hpp"

namespace koroutine::async_io {

// 将 std::ios::openmode 转换为平台相关的文件打开标志
inline int translate_mode(std::ios::openmode mode) {
#ifdef _WIN32
  // Windows 平台
  int flags = 0;
  if ((mode & std::ios::in) && (mode & std::ios::out)) {
    flags = _O_RDWR;
  } else if (mode & std::ios::in) {
    flags = _O_RDONLY;
  } else if (mode & std::ios::out) {
    flags = _O_WRONLY;
  }

  if (mode & std::ios::trunc) {
    flags |= _O_TRUNC;
  }
  if (mode & std::ios::app) {
    flags |= _O_APPEND;
  }
  if ((mode & std::ios::out) && !(mode & std::ios::in)) {
    flags |= _O_CREAT;
  }
  flags |= _O_BINARY;  // Windows 需要二进制模式
  return flags;
#else
  // POSIX 平台 (Linux, macOS)
  int flags = 0;
  if ((mode & std::ios::in) && (mode & std::ios::out)) {
    flags = O_RDWR;
  } else if (mode & std::ios::in) {
    flags = O_RDONLY;
  } else if (mode & std::ios::out) {
    flags = O_WRONLY;
  }

  if (mode & std::ios::trunc) {
    flags |= O_TRUNC;
  }
  if (mode & std::ios::app) {
    flags |= O_APPEND;
  }
  if ((mode & std::ios::out) && !(mode & std::ios::in)) {
    flags |= O_CREAT;
  }

  return flags;
#endif
}

class AsyncFile : public AsyncIOObject,
                  public std::enable_shared_from_this<AsyncFile> {
 public:
  static Task<std::shared_ptr<AsyncFile>> open(std::shared_ptr<IOEngine> engine,
                                               const std::string& path,
                                               std::ios::openmode mode) {
    LOG_TRACE("AsyncFile::open - opening file: ", path);
    // 平台相关的文件打开操作
#ifdef _WIN32
    // Windows 平台
    intptr_t fd =
        ::_open(path.c_str(), translate_mode(mode), _S_IREAD | _S_IWRITE);
    if (fd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to open file");
    }
#else
    // POSIX 平台 (Linux, macOS)
    int flags = translate_mode(mode);
#if defined(__APPLE__) || defined(__FreeBSD__)
    // macOS 和 FreeBSD 不支持 O_NONBLOCK 在 open() 中直接使用，需要使用 fcntl
    intptr_t fd = ::open(path.c_str(), flags, 0644);
    if (fd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to open file");
    }
    // 设置非阻塞模式
    int file_flags = ::fcntl(fd, F_GETFL, 0);
    if (file_flags == -1 ||
        ::fcntl(fd, F_SETFL, file_flags | O_NONBLOCK) == -1) {
      ::close(fd);
      throw std::system_error(errno, std::generic_category(),
                              "Failed to set non-blocking mode");
    }
#else
    // Linux 支持直接在 open() 中使用 O_NONBLOCK
    intptr_t fd = ::open(path.c_str(), flags | O_NONBLOCK, 0644);
    if (fd < 0) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to open file");
    }
#endif
#endif
    LOG_TRACE("AsyncFile::open - file opened successfully: ", path, fd);
    co_return std::make_shared<AsyncFile>(engine, fd);
  }

  AsyncFile(std::shared_ptr<IOEngine> engine, intptr_t fd)
      : AsyncIOObject(engine), fd_(fd) {}

  Task<size_t> read(void* buf, size_t size) override {
    LOG_TRACE("AsyncFile::read - reading file: ", fd_);
    auto io_op = std::make_shared<AsyncIOOp>(OpType::READ, shared_from_this(),
                                             buf, size);
    co_return co_await IOAwaiter<size_t>{io_op};
  }

  Task<size_t> write(const void* buf, size_t size) override {
    LOG_TRACE("AsyncFile::write - writing file: ", fd_);
    auto io_op = std::make_shared<AsyncIOOp>(OpType::WRITE, shared_from_this(),
                                             const_cast<void*>(buf), size);
    co_return co_await IOAwaiter<size_t>{io_op};
  }

  Task<void> close() override {
    LOG_TRACE("AsyncFile::close - closing file: ", fd_);
    auto io_op = std::make_shared<AsyncIOOp>(OpType::CLOSE, shared_from_this(),
                                             nullptr, 0);
    co_await IOAwaiter<void>{io_op};
  }

  intptr_t native_handle() const override { return fd_; }

  Task<void> seek(size_t position) {
    // 模拟seek操作
    co_return;
  }

  Task<void> flush() {
    // 模拟flush操作
    co_return;
  }

 private:
  intptr_t fd_;  // 文件描述符
};
}  // namespace koroutine::async_io
