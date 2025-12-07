#pragma once

#include <coroutine>
#include <optional>
#include <string>
#include <thread>

namespace koroutine {

/**
 * @brief 调度元数据 - 提供调度策略的额外信息
 */
struct ScheduleMetadata {
  /**
   * @brief 任务优先级
   */
  enum class Priority {
    Low = 0,     ///< 低优先级
    Normal = 1,  ///< 正常优先级（默认）
    High = 2     ///< 高优先级
  };

  Priority priority = Priority::Normal;     ///< 任务优先级
  std::optional<std::thread::id> affinity;  ///< 线程亲和性（可选）
  std::string debug_name;                   ///< 调试用名称（可选）

  // 默认构造
  ScheduleMetadata() = default;

  // 便捷构造：只设置优先级
  explicit ScheduleMetadata(Priority p) : priority(p) {}

  // 便捷构造：设置优先级和调试名称
  ScheduleMetadata(Priority p, std::string name)
      : priority(p), debug_name(std::move(name)) {}
};

/**
 * @brief 调度请求 - 封装协程句柄和调度元数据
 *
 * 这是调度器的核心输入类型，包含要调度的协程句柄以及相关的调度元数据。
 * 提供类型安全的协程调度接口。
 */
class ScheduleRequest {
 public:
  /**
   * @brief 构造调度请求
   * @param handle 要调度的协程句柄
   * @param meta 调度元数据（默认为普通优先级）
   */
  explicit ScheduleRequest(std::coroutine_handle<> handle,
                           ScheduleMetadata meta = {})
      : handle_(handle), metadata_(std::move(meta)) {}

  /**
   * @brief 获取协程句柄
   */
  std::coroutine_handle<> handle() const { return handle_; }

  /**
   * @brief 获取调度元数据
   */
  const ScheduleMetadata& metadata() const { return metadata_; }

  /**
   * @brief 恢复协程（便捷方法）
   */
  void resume() const {
    if (handle_) {
      LOG_TRACE("ScheduleRequest::resume - resuming handle: ",
                handle_.address());
      handle_.resume();
    }
  }

  /**
   * @brief 检查句柄是否有效
   */
  explicit operator bool() const { return handle_.operator bool(); }

 private:
  std::coroutine_handle<> handle_;
  ScheduleMetadata metadata_;
};

}  // namespace koroutine
