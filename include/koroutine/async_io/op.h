#pragma once
#include <coroutine>

#include "koroutine/async_io/io_object.h"
#include "koroutine/scheduler_manager.h"
#include "koroutine/schedulers/schedule_request.hpp"
#include "koroutine/schedulers/scheduler.h"

namespace koroutine::async_io {
class AsyncIOOp {
 public:
  OpType type;                                   // 操作类型
  std::shared_ptr<AsyncIOObject> io_object;      // 关联的IO对象
  void* buffer;                                  // 数据缓冲区
  size_t size;                                   // 请求的大小
  size_t actual_size;                            // 实际处理的大小
  std::error_code error;                         // 操作结果（错误码）
  std::coroutine_handle<> coro_handle;           // 协程句柄
  std::shared_ptr<AbstractScheduler> scheduler;  // 调度器指针

  AsyncIOOp(OpType op_type, std::shared_ptr<AsyncIOObject> obj, void* buf,
            size_t sz)
      : type(op_type),
        io_object(obj),
        buffer(buf),
        size(sz),
        actual_size(0),
        error() {
    scheduler = SchedulerManager::get_default_scheduler();
  }

  void complete() {
    if (!scheduler) {
      LOG_ERROR("AsyncIOOp::complete - no scheduler available!");
      if (coro_handle) {
        coro_handle.resume();  // Fallback: 直接恢复
      }
      return;
    }

    // 使用新的 ScheduleRequest 接口
    // IO 完成优先级设为 High，确保及时响应
    ScheduleMetadata meta(ScheduleMetadata::Priority::High, "io_completion");
    scheduler->schedule(ScheduleRequest(coro_handle, std::move(meta)), 0);
  }
};
}  // namespace koroutine::async_io