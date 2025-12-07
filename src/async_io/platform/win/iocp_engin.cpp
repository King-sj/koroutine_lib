#include "koroutine/async_io/platform/win/iocp_engin.h"

#if defined(_WIN64)
#include <mswsock.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>

#include "koroutine/async_io/op.h"
#include "koroutine/debug.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

namespace koroutine::async_io {

static LPFN_ACCEPTEX lpAcceptEx = nullptr;
static LPFN_CONNECTEX lpConnectEx = nullptr;
static LPFN_GETACCEPTEXSOCKADDRS lpGetAcceptExSockaddrs = nullptr;

static void load_mswsock_funcs() {
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) return;

  GUID guidAcceptEx = WSAID_ACCEPTEX;
  DWORD bytes = 0;
  WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx,
           sizeof(guidAcceptEx), &lpAcceptEx, sizeof(lpAcceptEx), &bytes, NULL,
           NULL);

  GUID guidConnectEx = WSAID_CONNECTEX;
  WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx,
           sizeof(guidConnectEx), &lpConnectEx, sizeof(lpConnectEx), &bytes,
           NULL, NULL);

  GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
  WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAcceptExSockaddrs,
           sizeof(guidGetAcceptExSockaddrs), &lpGetAcceptExSockaddrs,
           sizeof(lpGetAcceptExSockaddrs), &bytes, NULL, NULL);

  closesocket(sock);
}

IOCPIOEngine::IOCPIOEngine() : running_(false), iocp_handle_(NULL) {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    LOG_ERROR("WSAStartup failed: ", result);
    return;
  }

  iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (iocp_handle_ == NULL) {
    LOG_ERROR("CreateIoCompletionPort failed: ", GetLastError());
  }

  load_mswsock_funcs();
  running_ = true;
}

IOCPIOEngine::~IOCPIOEngine() {
  stop();
  if (iocp_handle_) {
    CloseHandle(iocp_handle_);
  }
  WSACleanup();
}

void IOCPIOEngine::submit(std::shared_ptr<AsyncIOOp> op) {
  if (!running_) {
    op->error = std::make_error_code(std::errc::operation_canceled);
    op->scheduler->schedule(op->coro_handle);
    return;
  }

  SOCKET fd = (SOCKET)op->io_object->native_handle();

  // Associate socket with IOCP if not already associated
  // Note: CreateIoCompletionPort can be called multiple times for the same
  // handle. It will just return the existing port.
  // However, we should be careful about the completion key.
  // Here we use the fd as the completion key.
  if (CreateIoCompletionPort((HANDLE)fd, iocp_handle_, (ULONG_PTR)fd, 0) ==
      NULL) {
    DWORD err = GetLastError();
    // ERROR_INVALID_PARAMETER might mean it's already associated with another
    // port? Or just already associated.
    // Usually it's fine.
    if (err != ERROR_INVALID_PARAMETER) {
      // LOG_WARN("CreateIoCompletionPort failed in submit: ", err);
    }
  }

  DWORD bytes_transferred = 0;
  int result = 0;
  DWORD flags = 0;

  switch (op->type) {
    case OpType::READ: {
      op->wsa_buf.buf = static_cast<char*>(op->buffer);
      op->wsa_buf.len = static_cast<ULONG>(op->size);
      op->flags = 0;
      result = WSARecv(fd, &op->wsa_buf, 1, &bytes_transferred, &op->flags,
                       &op->overlapped, NULL);
      break;
    }
    case OpType::WRITE: {
      op->wsa_buf.buf = static_cast<char*>(op->buffer);
      op->wsa_buf.len = static_cast<ULONG>(op->size);
      result = WSASend(fd, &op->wsa_buf, 1, &bytes_transferred, 0,
                       &op->overlapped, NULL);
      break;
    }
    case OpType::ACCEPT: {
      if (!lpAcceptEx || !lpGetAcceptExSockaddrs) {
        op->error = std::make_error_code(std::errc::function_not_supported);
        op->scheduler->schedule(op->coro_handle);
        return;
      }

      // Create a new socket for the connection
      // We need to know the family/type/protocol.
      // Assuming TCP/IPv4 for now, but should get from io_object if possible.
      // AsyncServerSocket doesn't expose it easily, but we can assume generic.
      // Or we can use the same family as the listening socket.
      // Let's assume AF_INET for now or try to detect.
      // For simplicity, let's create AF_INET.
      // TODO: Support IPv6 properly by checking listening socket.
      op->accept_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (op->accept_sock == INVALID_SOCKET) {
        op->error = std::make_error_code(std::errc::io_error);
        op->scheduler->schedule(op->coro_handle);
        return;
      }

      // Allocate buffer for addresses
      // sizeof(sockaddr_storage) + 16 for local and remote
      size_t addr_len = sizeof(sockaddr_storage) + 16;
      size_t buf_size = addr_len * 2;
      op->internal_buffer = std::make_unique<char[]>(buf_size);

      result =
          lpAcceptEx(fd, op->accept_sock, op->internal_buffer.get(),
                     0,  // dwReceiveDataLength = 0 (don't wait for data)
                     addr_len, addr_len, &bytes_transferred, &op->overlapped);
      break;
    }
    case OpType::CONNECT: {
      if (!lpConnectEx) {
        op->error = std::make_error_code(std::errc::function_not_supported);
        op->scheduler->schedule(op->coro_handle);
        return;
      }

      // ConnectEx requires the socket to be bound.
      struct sockaddr_in bind_addr;
      std::memset(&bind_addr, 0, sizeof(bind_addr));
      bind_addr.sin_family = AF_INET;
      bind_addr.sin_addr.s_addr = INADDR_ANY;
      bind_addr.sin_port = 0;
      if (bind(fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
        // If already bound, it might fail, which is fine?
        // But usually we should bind if not bound.
      }

      // op->buffer should contain the target address (sockaddr*)
      // op->size should be the length
      const struct sockaddr* name = (const struct sockaddr*)op->buffer;
      int namelen = (int)op->size;

      result = lpConnectEx(fd, name, namelen, NULL, 0, &bytes_transferred,
                           &op->overlapped);
      break;
    }
    case OpType::CLOSE: {
      // Just close the socket.
      // This will cancel pending IOs.
      closesocket(fd);
      // We should schedule the coroutine immediately as there's no async
      // completion for close (unless we use DisconnectEx?)
      // But AsyncIOOp expects a completion.
      // We can PostQueuedCompletionStatus to simulate completion.
      PostQueuedCompletionStatus(iocp_handle_, 0, (ULONG_PTR)fd,
                                 &op->overlapped);
      return;
    }
    default:
      op->error = std::make_error_code(std::errc::operation_not_supported);
      op->scheduler->schedule(op->coro_handle);
      return;
  }

  if (result == FALSE) {
    int error = WSAGetLastError();
    if (error != ERROR_IO_PENDING) {
      LOG_ERROR("IOCP submit failed: ", error);
      op->error = std::make_error_code(std::errc::io_error);
      op->scheduler->schedule(op->coro_handle);
    }
  }
}

void IOCPIOEngine::run() {
  DWORD bytes_transferred;
  ULONG_PTR completion_key;
  LPOVERLAPPED overlapped;

  while (running_) {
    BOOL result = GetQueuedCompletionStatus(iocp_handle_, &bytes_transferred,
                                            &completion_key, &overlapped, 100);

    if (result == FALSE && overlapped == NULL) {
      // Timeout or error not related to an IO operation
      if (GetLastError() != WAIT_TIMEOUT) {
        // LOG_ERROR("GetQueuedCompletionStatus failed: ", GetLastError());
      }
      continue;
    }

    if (overlapped == NULL) {
      continue;
    }

    // Retrieve AsyncIOOp from OVERLAPPED
    // Since overlapped is a member of AsyncIOOp, we can cast.
    // But we need to be careful about offset.
    // AsyncIOOp* op = CONTAINING_RECORD(overlapped, AsyncIOOp, overlapped);
    // Since we don't have CONTAINING_RECORD available easily without windows
    // headers macros which might conflict, let's calculate manually or assume
    // standard layout.
    // Actually, offsetof is standard.
    AsyncIOOp* op = reinterpret_cast<AsyncIOOp*>(
        reinterpret_cast<char*>(overlapped) - offsetof(AsyncIOOp, overlapped));

    if (result == FALSE) {
      // IO failed
      op->error = std::make_error_code(std::errc::io_error);
      op->actual_size = 0;
    } else {
      op->actual_size = bytes_transferred;
      op->error = std::error_code();

      if (op->type == OpType::ACCEPT) {
        // Handle AcceptEx completion
        // Update context
        setsockopt(op->accept_sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                   (char*)&completion_key, sizeof(completion_key));

        // Parse addresses
        sockaddr *local_addr = nullptr, *remote_addr = nullptr;
        int local_len = 0, remote_len = 0;
        size_t addr_len = sizeof(sockaddr_storage) + 16;

        lpGetAcceptExSockaddrs(op->internal_buffer.get(), 0, addr_len, addr_len,
                               &local_addr, &local_len, &remote_addr,
                               &remote_len);

        // Copy remote address to op->buffer (client_addr)
        if (op->buffer && op->size >= (size_t)remote_len) {
          std::memcpy(op->buffer, remote_addr, remote_len);
          // We might want to update the length if passed by pointer, but
          // AsyncIOOp doesn't have a pointer to length.
        }

        // Return the new socket fd
        op->actual_size = (size_t)op->accept_sock;
      } else if (op->type == OpType::CONNECT) {
        // Update connect context
        setsockopt((SOCKET)op->io_object->native_handle(), SOL_SOCKET,
                   SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
      }
    }

    // Resume coroutine
    if (op->scheduler && op->coro_handle) {
      op->scheduler->schedule(op->coro_handle);
    }
  }
}

void IOCPIOEngine::stop() {
  running_ = false;
  if (iocp_handle_) {
    PostQueuedCompletionStatus(iocp_handle_, 0, 0, NULL);
  }
}

bool IOCPIOEngine::is_running() { return running_; }

}  // namespace koroutine::async_io

#endif  // _WIN64
