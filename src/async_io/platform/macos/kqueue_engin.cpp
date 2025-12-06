#if defined(__APPLE__)
#include "koroutine/async_io/platform/macos/kqueue_engin.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <system_error>

#include "koroutine/async_io/engin.h"
#include "koroutine/debug.h"

namespace koroutine::async_io {

KqueueIOEngine::KqueueIOEngine() : running_(false) {
  // 创建 kqueue 文件描述符
  kqueue_fd_ = kqueue();
  if (kqueue_fd_ == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create kqueue");
  }
}

KqueueIOEngine::~KqueueIOEngine() {
  LOG_TRACE("KqueueIOEngine::~KqueueIOEngine - destroying KqueueIOEngine");
  stop();
  LOG_TRACE("KqueueIOEngine::~KqueueIOEngine - kqueue closed");
  if (kqueue_fd_ != -1) {
    ::close(kqueue_fd_);
  }
}

void KqueueIOEngine::submit(std::shared_ptr<AsyncIOOp> op) {
  LOG_TRACE("KqueueIOEngine::submit - submitting IO operation of type ",
            static_cast<int>(op->type));
  std::lock_guard<std::mutex> lock(ops_mutex_);
  LOG_TRACE("KqueueIOEngine::submit - operation queued");
  pending_ops_.push(op);
}

void KqueueIOEngine::run() {
  running_.store(true);

  constexpr int MAX_EVENTS = 64;
  struct kevent events[MAX_EVENTS];
  LOG_INFO("KqueueIOEngine::run - starting event loop");
  while (running_.load()) {
    // 处理待提交的操作
    {
      LOG_TRACE("KqueueIOEngine::run - processing pending operations");
      //   TODO: 细化 ops_mutex_ 的锁粒度，避免阻塞过久
      // 操作完 pending_ops_ 后立即释放锁
      std::lock_guard<std::mutex> lock(ops_mutex_);
      while (!pending_ops_.empty()) {
        auto op = pending_ops_.front();
        pending_ops_.pop();

        switch (op->type) {
          case OpType::READ:
          case OpType::ACCEPT:
          case OpType::RECVFROM:
            process_read(op);
            break;
          case OpType::WRITE:
          case OpType::CONNECT:
          case OpType::SENDTO:
            process_write(op);
            break;
          case OpType::CLOSE:
            process_close(op);
            break;
          default:
            op->error = std::make_error_code(std::errc::not_supported);
            complete(op);
            break;
        }
      }
    }

    // 等待 kqueue 事件，超时时间 100ms
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100'000'000;  // 100ms

    int nev = kevent(kqueue_fd_, nullptr, 0, events, MAX_EVENTS, &timeout);

    if (nev < 0) {
      if (errno == EINTR) {
        LOG_DEBUG("KqueueIOEngine::run - kevent interrupted by signal");
        continue;  // 被信号中断，继续循环
      }
      LOG_ERROR("KqueueIOEngine::run - kevent error: ", errno);
      // 其他错误
      throw std::system_error(errno, std::generic_category(), "kevent failed");
    }

    // 处理就绪的事件
    LOG_TRACE("KqueueIOEngine::run - processing event ", nev);
    for (int i = 0; i < nev; ++i) {
      auto& event = events[i];
      intptr_t fd = static_cast<intptr_t>(event.ident);

      auto it = active_ops_.find(fd);
      if (it == active_ops_.end()) {
        LOG_WARN("KqueueIOEngine::run - no active operation for fd ", fd);
        continue;  // 找不到对应的操作，跳过
      }

      auto op = it->second;

      // 检查是否有错误
      if (event.flags & EV_ERROR) {
        op->error = std::make_error_code(std::errc::io_error);
        op->actual_size = 0;
        LOG_ERROR("KqueueIOEngine::run - IO error occurred for fd ", fd);
      } else {
        // 根据操作类型执行实际的 IO
        if (op->type == OpType::READ) {
          ssize_t n = ::read(fd, op->buffer, op->size);
          if (n < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(errno));
            op->actual_size = 0;
          } else {
            op->actual_size = static_cast<size_t>(n);
          }
        } else if (op->type == OpType::WRITE) {
          ssize_t n = ::write(fd, op->buffer, op->size);
          if (n < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(errno));
            op->actual_size = 0;
          } else {
            op->actual_size = static_cast<size_t>(n);
          }
        } else if (op->type == OpType::ACCEPT) {
          socklen_t addrlen = static_cast<socklen_t>(op->size);
          int client_fd =
              ::accept(static_cast<int>(fd),
                       static_cast<struct sockaddr*>(op->buffer), &addrlen);
          if (client_fd < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(errno));
            op->actual_size = 0;
          } else {
            // Set non-blocking for the new socket
            int flags = ::fcntl(client_fd, F_GETFL, 0);
            ::fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
            op->actual_size = static_cast<size_t>(client_fd);
          }
        } else if (op->type == OpType::CONNECT) {
          int error = 0;
          socklen_t len = sizeof(error);
          if (::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_ERROR, &error,
                           &len) < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(errno));
          } else if (error != 0) {
            op->error = std::make_error_code(static_cast<std::errc>(error));
          } else {
            op->actual_size = 0;
          }
        } else if (op->type == OpType::RECVFROM) {
          op->addr_len = sizeof(op->addr);
          ssize_t n = ::recvfrom(static_cast<int>(fd), op->buffer, op->size, 0,
                                 (struct sockaddr*)&op->addr, &op->addr_len);
          if (n < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(errno));
            op->actual_size = 0;
          } else {
            op->actual_size = static_cast<size_t>(n);
          }
        } else if (op->type == OpType::SENDTO) {
          ssize_t n = ::sendto(static_cast<int>(fd), op->buffer, op->size, 0,
                               (const struct sockaddr*)&op->addr, op->addr_len);
          if (n < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(errno));
            op->actual_size = 0;
          } else {
            op->actual_size = static_cast<size_t>(n);
          }
        }
      }

      // 从 active_ops_ 中移除
      active_ops_.erase(it);

      // 完成操作，恢复协程
      LOG_TRACE("KqueueIOEngine::run - completing IO operation for fd ", fd);
      complete(op);
      LOG_TRACE("KqueueIOEngine::run - IO operation completed for fd ", fd);
    }
  }
}

void KqueueIOEngine::stop() {
  LOG_INFO("KqueueIOEngine::stop - stopping event loop");
  running_.store(false);
}

bool KqueueIOEngine::is_running() { return running_.load(); }

void KqueueIOEngine::process_read(std::shared_ptr<AsyncIOOp> op) {
  LOG_INFO("KqueueIOEngine::process_read - processing read operation");
  intptr_t fd = op->io_object->native_handle();

  // 注册读事件到 kqueue
  struct kevent kev;
  EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, nullptr);

  if (kevent(kqueue_fd_, &kev, 1, nullptr, 0, nullptr) == -1) {
    op->error = std::make_error_code(static_cast<std::errc>(errno));
    complete(op);
    return;
  }

  // 记录活动的操作
  active_ops_[fd] = op;
}

void KqueueIOEngine::process_write(std::shared_ptr<AsyncIOOp> op) {
  LOG_INFO("KqueueIOEngine::process_write - processing write operation");
  intptr_t fd = op->io_object->native_handle();

  // 注册写事件到 kqueue
  struct kevent kev;
  EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, nullptr);

  if (kevent(kqueue_fd_, &kev, 1, nullptr, 0, nullptr) == -1) {
    op->error = std::make_error_code(static_cast<std::errc>(errno));
    complete(op);
    return;
  }

  // 记录活动的操作
  active_ops_[fd] = op;
}

void KqueueIOEngine::process_close(std::shared_ptr<AsyncIOOp> op) {
  LOG_INFO("KqueueIOEngine::process_close - processing close operation");
  intptr_t fd = op->io_object->native_handle();

  // 从 kqueue 中删除该文件描述符的所有监听
  struct kevent kev[2];
  EV_SET(&kev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  EV_SET(&kev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  kevent(kqueue_fd_, kev, 2, nullptr, 0, nullptr);

  // 关闭文件描述符
  if (::close(fd) == -1) {
    op->error = std::make_error_code(static_cast<std::errc>(errno));
  }

  // 从 active_ops_ 中移除
  active_ops_.erase(fd);

  // 完成操作
  complete(op);
}

}  // namespace koroutine::async_io
#endif