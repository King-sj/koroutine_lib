#if defined(__APPLE__)
#include "koroutine/async_io/platform/macos/kqueue_engin.h"

#include "koroutine/async_io/engin.h"

namespace koroutine::async_io {
KqueueIOEngine::KqueueIOEngine() {
  // 初始化kqueue相关资源
}

KqueueIOEngine::~KqueueIOEngine() {
  // 清理kqueue相关资源
}

void KqueueIOEngine::submit(std::shared_ptr<AsyncIOOp> op) {
  // 提交异步IO操作到kqueue
}

void KqueueIOEngine::run() {
  // 运行kqueue事件循环
}

void KqueueIOEngine::stop() {
  // 停止kqueue事件循环
}
bool KqueueIOEngine::is_running() {
  // 检查kqueue事件循环是否在运行
  return false;
}

}  // namespace koroutine::async_io
#endif