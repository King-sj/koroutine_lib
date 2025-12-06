#define KOROUTINE_DEBUG
#include <iostream>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server() {
  auto svr = std::make_shared<Server>();

  // Serve static files from the current directory
  // You can change "." to any directory you want to serve
  auto ret = svr->set_mount_point("/", ".");
  if (!ret) {
    std::cerr << "The specified base directory doesn't exist..." << std::endl;
    co_return;
  }

  svr->Get("/hi", [](const Request&, Response& res) -> Task<void> {
    res.set_content("Hello World!", "text/plain");
    co_return;
  });

  std::cout << "File Server listening on http://0.0.0.0:8080" << std::endl;
  std::cout << "Try accessing http://0.0.0.0:8080/CMakeLists.txt" << std::endl;

  co_await svr->listen_async("0.0.0.0", 8080);
}

int main() {
  debug::set_level(debug::Level::Info);
  Runtime::block_on(run_server());
  return 0;
}
