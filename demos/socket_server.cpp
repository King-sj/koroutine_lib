#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

// Store active tasks to keep them alive
std::vector<std::shared_ptr<Task<void>>> client_tasks;

Task<void> handle_client(std::shared_ptr<AsyncSocket> socket) {
  try {
    char buffer[1024];
    while (true) {
      size_t n = co_await socket->read(buffer, sizeof(buffer));
      if (n == 0) break;  // Connection closed

      std::string msg(buffer, n);
      std::cout << "Received: " << msg << std::endl;

      co_await socket->write(buffer, n);  // Echo back
    }
    co_await socket->close();
  } catch (const std::exception& e) {
    std::cerr << "Client error: " << e.what() << std::endl;
  }
  std::cout << "Client disconnected" << std::endl;
}

Task<void> run_server() {
  auto server = co_await AsyncServerSocket::bind(8080);
  std::cout << "Server listening on port 8080..." << std::endl;

  while (true) {
    auto client = co_await server->accept();
    std::cout << "New client connected!" << std::endl;

    // Create a task and store it
    auto task = std::make_shared<Task<void>>(handle_client(client));
    task->start();  // Start the task!
    client_tasks.push_back(task);

    // Simple cleanup of finished tasks
    // Note: In a real server, you'd want a better way to manage task lifecycle
    // and avoid iterating the whole list frequently.
    auto it = std::remove_if(client_tasks.begin(), client_tasks.end(),
                             [](const auto& t) { return t->is_done(); });
    client_tasks.erase(it, client_tasks.end());
  }
}

int main() {
  Runtime::block_on(run_server());
  return 0;
}
