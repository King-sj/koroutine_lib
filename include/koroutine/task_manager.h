#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "cancellation.hpp"
#include "task.hpp"

namespace koroutine {
/**
 * @brief TaskManager class to manage multiple tasks
 */
class TaskManager {
 private:
  struct TaskEntry {
    std::shared_ptr<Task<void>> task;
    CancellationTokenSource cts;
    std::string name;
  };

  // Protects groups_ and is_shutdown_
  mutable std::mutex mtx_;
  std::unordered_map<std::string, std::vector<TaskEntry>> groups_;
  bool is_shutdown_ = false;

 public:
  TaskManager() = default;
  ~TaskManager();
  /**
   * @brief Submit a task to a named group, the task will start immediately
   * @param name The name of the task group
   * @param task The task to be submitted
   */
  virtual void submit_to_group(const std::string& name,
                               std::shared_ptr<Task<void>> task);
  /**
   * @brief Wait for all tasks in a named group to complete
   * @param name The name of the task group
   * @return A task that completes when all tasks in the group have completed
   */
  Task<void> join_group(std::string name);
  /**
   * @brief Synchronously wait for all tasks in a named group to complete
   * @param name The name of the task group, if empty, waits for all tasks
   */
  void sync_wait_group(const std::string& name);
  Task<void> shutdown();
  void sync_shutdown();
  void sync_cancel_group(const std::string& name);
  /**
   * @brief Cancel all tasks in a named group
   * @param name The name of the task group, if empty, cancels all tasks
   * @return A task that completes when all tasks in the group have been
   * cancelled
   */
  Task<void> cancel_group(std::string name);

  /**
   * @brief List groups and their active task counts
   */
  std::vector<std::pair<std::string, size_t>> list_groups() const;
};

}  // namespace koroutine
