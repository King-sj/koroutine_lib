// #define KOROUTINE_DEBUG
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/debug.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

TaskManager manager;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (tokenStream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

Task<void> background_task(std::string name, int interval_sec,
                           int duration_sec) {
  auto start_time = std::chrono::steady_clock::now();

  try {
    co_await (cout << "Task [" << name << "] is running\n");
    while (true) {
      if (duration_sec > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
                .count();
        if (elapsed >= duration_sec) {
          break;
        }
      }

      // Cancellation is signaled via the promise await_transform, so sleep may
      // throw OperationCancelledException when cancelled.
      co_await (cout << "Task [" << name << "] tick\n");
      co_await SleepAwaiter(interval_sec * 1000);
    }
  } catch (const OperationCancelledException& e) {
    std::cerr << "Task [" << name << "] cancelled\n";
  } catch (...) {
    // Handle cancellation or other errors
    std::cerr << "Task [" << name << "] cancelled or error occurred\n";
  }
  co_await (cout << "Task [" << name << "] finished\n");
}

Task<void> run_cli() {
  co_await (cout << "Task Manager Demo\n");
  co_await (cout << "Commands:\n");
  co_await (cout << "  start <name> [interval_sec] [duration_sec]\n");
  co_await (cout << "  cancel <name>\n");
  co_await (cout << "  list\n");
  co_await (cout << "  exit\n");

  std::string line_buf;
  while (true) {
    co_await (cout << "> ");

    line_buf.clear();
    char c;
    bool eof = false;
    while (true) {
      size_t n = co_await cin.read(&c, 1);
      if (n == 0) {
        eof = true;
        break;
      }
      if (c == '\n') break;
      line_buf.push_back(c);
    }

    if (line_buf.empty()) {
      if (eof) break;  // Exit on EOF
      continue;
    }
    auto tokens = split(line_buf);
    if (tokens.empty()) continue;

    std::string cmd = tokens[0];

    if (cmd == "exit") {
      co_await (cout << "Stopping all tasks...\n");

      manager.sync_shutdown();

      // everything is stopped by manager
      break;
    } else if (cmd == "start") {
      if (tokens.size() < 2) {
        co_await (
            cout << "Usage: start <name> [interval_sec] [duration_sec]\n");
        continue;
      }
      std::string name = tokens[1];
      int interval = 1;
      int duration = -1;
      if (tokens.size() >= 3) interval = std::stoi(tokens[2]);
      if (tokens.size() >= 4) duration = std::stoi(tokens[3]);

      // If group is running, refuse to start a duplicate
      auto groups = manager.list_groups();
      bool exists = false;
      for (auto& p : groups) {
        if (p.first == name) {
          exists = true;
          break;
        }
      }
      if (exists) {
        co_await (cout << "Task " << name << " is already running.\n");
        continue;
      }

      auto t_ptr = std::make_shared<Task<void>>(
          background_task(name, interval, duration));
      // submit and manager will set cancellation token and start
      manager.submit_to_group(name, t_ptr);
      co_await (cout << "Started task " << name << "\n");

    } else if (cmd == "cancel") {
      if (tokens.size() < 2) {
        co_await (cout << "Usage: cancel <name>\n");
        continue;
      }
      std::string name = tokens[1];
      // Manager will cancel group
      manager.sync_cancel_group(name);
      co_await (cout << "Signal sent to cancel task " << name << "\n");
    } else if (cmd == "list") {
      co_await (cout << "Current Tasks:\n");
      auto groups = manager.list_groups();
      for (auto& g : groups) {
        co_await (cout << "  " << g.first << " (" << g.second << " Running)\n");
      }
    } else {
      co_await (cout << "Unknown command: " << cmd << "\n");
    }
  }
}

int main() {
  Runtime::block_on(run_cli());
  return 0;
}
