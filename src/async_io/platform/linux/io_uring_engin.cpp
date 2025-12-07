#include "koroutine/async_io/platform/linux/io_uring_engin.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <system_error>

#include "koroutine/debug.h"

namespace koroutine::async_io {

// Special pointer value to identify wakeup events
// 用于标识唤醒事件的特殊指针值
static const uintptr_t WAKEUP_USER_DATA = 0xFFFFFFFFFFFFFFFF;

// io_uring 队列深度
static const unsigned IO_URING_QUEUE_DEPTH = 256;

IoUringIOEngine::IoUringIOEngine() : running_(false) {
  // 初始化 io_uring，设置队列深度
  if (io_uring_queue_init(IO_URING_QUEUE_DEPTH, &ring_, 0) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to initialize io_uring");
  }

  // 创建 eventfd 用于唤醒事件循环
  event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (event_fd_ == -1) {
    io_uring_queue_exit(&ring_);
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create eventfd");
  }
}

IoUringIOEngine::~IoUringIOEngine() {
  stop();
  if (event_fd_ != -1) {
    close(event_fd_);
  }
  io_uring_queue_exit(&ring_);
}

void IoUringIOEngine::submit(std::shared_ptr<AsyncIOOp> op) {
  {
    std::lock_guard<std::mutex> lock(ops_mutex_);
    pending_ops_.push(op);
  }

  // 唤醒事件循环
  uint64_t val = 1;
  if (write(event_fd_, &val, sizeof(val)) == -1) {
    // 忽略 EAGAIN，记录其他错误
    if (errno != EAGAIN) {
      LOG_ERROR("Failed to write to eventfd: ", errno);
    }
  }
}

void IoUringIOEngine::run() {
  running_.store(true);

  uint64_t ev_buf = 0;
  struct iovec ev_iov;
  ev_iov.iov_base = &ev_buf;
  ev_iov.iov_len = sizeof(ev_buf);

  // 初始提交 eventfd 读操作
  {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (sqe) {
      io_uring_prep_read(sqe, event_fd_, &ev_buf, sizeof(ev_buf), 0);
      io_uring_sqe_set_data(sqe, (void*)WAKEUP_USER_DATA);
      io_uring_submit(&ring_);
    }
  }

  while (running_.load()) {
    // 处理可能在唤醒前添加的待处理操作

    struct io_uring_cqe* cqe;
    // 等待至少一个事件
    int ret = io_uring_submit_and_wait(&ring_, 1);
    if (ret < 0) {
      if (ret == -EINTR) continue;
      LOG_ERROR("io_uring_submit_and_wait failed: ", -ret);
      break;
    }

    unsigned head;
    unsigned count = 0;
    io_uring_for_each_cqe(&ring_, head, cqe) {
      count++;
      uintptr_t user_data = (uintptr_t)io_uring_cqe_get_data(cqe);
      int res = cqe->res;

      if (user_data == WAKEUP_USER_DATA) {
        // 唤醒事件
        if (res < 0) {
          LOG_ERROR("Eventfd read failed: ", -res);
        }

        // 处理待处理的操作
        std::queue<std::shared_ptr<AsyncIOOp>> ops_to_process;
        {
          std::lock_guard<std::mutex> lock(ops_mutex_);
          ops_to_process.swap(pending_ops_);
        }

        while (!ops_to_process.empty()) {
          process_op(ops_to_process.front());
          ops_to_process.pop();
        }

        // 重新提交 eventfd 读操作
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (sqe) {
          io_uring_prep_read(sqe, event_fd_, &ev_buf, sizeof(ev_buf), 0);
          io_uring_sqe_set_data(sqe, (void*)WAKEUP_USER_DATA);
        }
      } else {
        // 用户操作
        auto* op_ptr = reinterpret_cast<AsyncIOOp*>(user_data);
        auto it = in_flight_ops_.find(op_ptr);
        if (it != in_flight_ops_.end()) {
          auto op = it->second;
          in_flight_ops_.erase(it);

          if (res < 0) {
            op->error = std::make_error_code(static_cast<std::errc>(-res));
            op->actual_size = 0;
          } else {
            op->actual_size = static_cast<size_t>(res);
            op->error = std::error_code();

            if (op->type == OpType::RECVFROM) {
              op->addr_len = op->msg.msg_namelen;
            }
          }
          complete(op);
        }
      }
    }
    io_uring_cq_advance(&ring_, count);
  }
}

void IoUringIOEngine::process_op(std::shared_ptr<AsyncIOOp> op) {
  struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    // 队列已满？应该处理这种情况。目前先记录日志并失败。
    // 理想情况下应该将其保留在 pending 中并重试。
    LOG_ERROR("io_uring SQ full");
    op->error = std::make_error_code(std::errc::no_buffer_space);
    complete(op);
    return;
  }

  int fd = op->io_object->native_handle();

  switch (op->type) {
    case OpType::READ:
      // Use offset -1 to use current file position
      io_uring_prep_read(sqe, fd, op->buffer, op->size, -1);
      break;
    case OpType::WRITE:
      // Use offset -1 to use current file position
      io_uring_prep_write(sqe, fd, op->buffer, op->size, -1);
      break;
    case OpType::ACCEPT:
      // 初始化 addr_len 为缓冲区大小（通常是 sizeof(sockaddr_storage)）
      // 或者用户提供的任何大小。假设 op->size 保存地址缓冲区的最大大小。
      op->addr_len = static_cast<socklen_t>(op->size);
      io_uring_prep_accept(sqe, fd, (struct sockaddr*)op->buffer,
                           (socklen_t*)&op->addr_len, 0);
      break;
    case OpType::CONNECT:
      // 对于 connect，如果 buffer 为空（意味着 connect 已经以非阻塞方式发起），
      // 我们使用 poll_add 等待 POLLOUT。
      if (op->buffer == nullptr) {
        io_uring_prep_poll_add(sqe, fd, POLLOUT);
      } else {
        io_uring_prep_connect(sqe, fd, (struct sockaddr*)op->buffer,
                              static_cast<socklen_t>(op->size));
      }
      break;
    // ... other cases
    case OpType::RECVFROM:
      op->iov.iov_base = op->buffer;
      op->iov.iov_len = op->size;
      op->msg.msg_name = &op->addr;
      op->msg.msg_namelen = sizeof(op->addr);
      op->msg.msg_iov = &op->iov;
      op->msg.msg_iovlen = 1;
      op->msg.msg_control = nullptr;
      op->msg.msg_controllen = 0;
      op->msg.msg_flags = 0;
      io_uring_prep_recvmsg(sqe, fd, &op->msg, 0);
      break;
    case OpType::SENDTO:
      op->iov.iov_base = op->buffer;
      op->iov.iov_len = op->size;
      op->msg.msg_name = &op->addr;
      op->msg.msg_namelen = op->addr_len;
      op->msg.msg_iov = &op->iov;
      op->msg.msg_iovlen = 1;
      op->msg.msg_control = nullptr;
      op->msg.msg_controllen = 0;
      op->msg.msg_flags = 0;
      io_uring_prep_sendmsg(sqe, fd, &op->msg, 0);
      break;
    case OpType::CLOSE:
      io_uring_prep_close(sqe, fd);
      break;
    default:
      // 回退或错误
      LOG_ERROR("Unsupported OpType in IoUringIOEngine: ", (int)op->type);
      op->error = std::make_error_code(std::errc::operation_not_supported);
      complete(op);
      io_uring_prep_nop(sqe);
      io_uring_sqe_set_data(sqe, nullptr);
      return;
  }

  // 通用设置
  io_uring_sqe_set_data(sqe, op.get());
  in_flight_ops_[op.get()] = op;
}

void IoUringIOEngine::stop() {
  running_.store(false);
  uint64_t val = 1;
  write(event_fd_, &val, sizeof(val));
}

bool IoUringIOEngine::is_running() { return running_.load(); }

}  // namespace koroutine::async_io
