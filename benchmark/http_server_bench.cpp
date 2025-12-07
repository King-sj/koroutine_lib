#include <iostream>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server(int port) {
  auto svr = std::make_shared<Server>();

  // Simple endpoint for benchmarking
  svr->Get("/hi", [](const Request&, Response& res) -> Task<void> {
    res.set_content("Hello World!", "text/plain");
    co_return;
  });

  std::cout << "Benchmark Server listening on port " << port << std::endl;

  // Disable logging for performance
  debug::set_level(debug::Level::None);

  bool ret = co_await svr->listen_async("0.0.0.0", port);
  if (!ret) {
    std::cerr << "Failed to listen on port " << port << std::endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  int port = 8080;
  if (argc > 1) {
    port = std::atoi(argv[1]);
  }

  // Ensure logging is off globally before starting
  debug::set_level(debug::Level::None);

  // Use default scheduler (ThreadPoolExecutor)

  Runtime::block_on(run_server(port));
  return 0;
}
