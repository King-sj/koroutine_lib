#include "koroutine/task_manager.h"

#include <algorithm>
#include <iostream>

#include "koroutine/awaiters/sleep_awaiter.hpp"
#include "koroutine/debug.h"
#include "koroutine/runtime.hpp"

namespace koroutine {

TaskManager::~TaskManager() {
  // Try to gracefully shutdown
  try {
    sync_shutdown();
  } catch (...) {
    LOG_WARN("TaskManager::~TaskManager - shutdown encountered an exception");
  }
}

void TaskManager::submit_to_group(const std::string& name,
                                  std::shared_ptr<Task<void>> task) {
  std::lock_guard lock(mtx_);
  if (is_shutdown_) {
    LOG_WARN("TaskManager::submit_to_group - manager is shutdown, ignoring");
    return;
  }

  TaskEntry entry;
  entry.task = task;
  entry.name = name;

  // Install cancellation token to the task
  entry.task->with_cancellation(entry.cts.token());

  // Start and add
  entry.task->start();
  groups_[name].push_back(std::move(entry));

  LOG_TRACE("TaskManager::submit_to_group - submitted task to group: ", name,
            " total=", groups_[name].size());
}

Task<void> TaskManager::join_group(std::string name) {
  return [](TaskManager* self, std::string groupName) -> Task<void> {
    while (true) {
      // Build a list of active tasks without holding the lock while awaiting
      // Debug print
      LOG_TRACE("TaskManager::join_group - scanning groups for name: ",
                groupName);
      std::vector<std::weak_ptr<Task<void>>> active;
      {
        std::lock_guard lock(self->mtx_);

        if (groupName.empty()) {
          // all groups
          for (auto& [gname, vec] : self->groups_) {
            for (auto& e : vec) {
              if (e.task && !e.task->is_done()) active.push_back(e.task);
            }
          }
        } else {
          auto it = self->groups_.find(groupName);
          if (it != self->groups_.end()) {
            for (auto& e : it->second) {
              if (e.task && !e.task->is_done()) active.push_back(e.task);
            }
          }
        }
      }

      if (active.empty()) break;  // no active tasks, done

      // Remove finished tasks from groups to avoid growth (cleanup)
      {
        std::lock_guard lock(self->mtx_);
        if (groupName.empty()) {
          for (auto it = self->groups_.begin(); it != self->groups_.end();) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [](const TaskEntry& e) {
                                       return !e.task || e.task->is_done();
                                     }),
                      vec.end());
            if (vec.empty())
              it = self->groups_.erase(it);
            else
              ++it;
          }
        } else {
          auto git = self->groups_.find(groupName);
          if (git != self->groups_.end()) {
            auto& vec = git->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [](const TaskEntry& e) {
                                       return !e.task || e.task->is_done();
                                     }),
                      vec.end());
            if (vec.empty()) self->groups_.erase(git);
          }
        }
      }

      co_await SleepAwaiter(20);
    }

    co_return;
  }(this, std::move(name));
}

void TaskManager::sync_wait_group(const std::string& name) {
  Runtime::block_on(join_group(name));
}

Task<void> TaskManager::cancel_group(std::string name) {
  return [](TaskManager* self, std::string groupName) -> Task<void> {
    {
      std::lock_guard lock(self->mtx_);
      if (groupName.empty()) {
        for (auto& [gname, vec] : self->groups_) {
          for (auto& e : vec) {
            e.cts.cancel();
          }
        }
      } else {
        auto it = self->groups_.find(groupName);
        if (it != self->groups_.end()) {
          for (auto& e : it->second) e.cts.cancel();
        }
      }
    }

    // Wait until all related tasks are gone
    co_await self->join_group(groupName);
    co_return;
  }(this, std::move(name));
}

void TaskManager::sync_cancel_group(const std::string& name) {
  Runtime::block_on(cancel_group(name));
}

Task<void> TaskManager::shutdown() {
  return [](TaskManager* self) -> Task<void> {
    {
      std::lock_guard lock(self->mtx_);
      if (self->is_shutdown_) {
        co_return;
      }
      self->is_shutdown_ = true;
    }

    // cancel all groups and wait for them
    co_await self->cancel_group("");
    co_return;
  }(this);
}

void TaskManager::sync_shutdown() { Runtime::block_on(shutdown()); }

std::vector<std::pair<std::string, size_t>> TaskManager::list_groups() const {
  std::lock_guard lock(mtx_);
  std::vector<std::pair<std::string, size_t>> res;
  for (auto& [name, vec] : groups_) {
    size_t cnt = std::count_if(vec.begin(), vec.end(), [](auto& e) {
      return e.task && !e.task->is_done();
    });
    if (cnt > 0) res.emplace_back(name, cnt);
  }
  return res;
}

}  // namespace koroutine
