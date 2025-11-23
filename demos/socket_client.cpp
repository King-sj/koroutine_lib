#include <iostream>
#include <string>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

Task<void> run_client() {
  try {
    std::cout << "Connecting to server..." << std::endl;
    auto socket = co_await AsyncSocket::connect("127.0.0.1", 8080);
    std::cout << "Connected!" << std::endl;

    std::string message = "Hello, Koroutine!";
    co_await socket->write(message.data(), message.size());
    std::cout << "Sent: " << message << std::endl;

    char buffer[1024];
    size_t n = co_await socket->read(buffer, sizeof(buffer));
    std::string response(buffer, n);
    std::cout << "Received: " << response << std::endl;

    co_await socket->close();
  } catch (const std::exception& e) {
    std::cerr << "Client error: " << e.what() << std::endl;
  }
}

int main() {
  Runtime::block_on(run_client());
  return 0;
}
