#define KOROUTINE_DEBUG
#include <chrono>
#include <iostream>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server() {
  // We need to keep the server alive
  auto svr = std::make_shared<Server>();

  svr->Get("/hi", [](const Request& req, Response& res) -> Task<void> {
    LOG_ERROR("Handling /hi request");
    res.set_content("Hello World!", "text/plain");
    co_return;
  });

  std::cout << "Server listening on http://localhost:8080" << std::endl;

  // listen_async should be an infinite loop (until stop)
  // We await it here, so this task will run forever.
  bool ret = co_await svr->listen_async("0.0.0.0", 8080);

  if (!ret) {
    std::cerr << "Failed to listen" << std::endl;
  }
}

int main() {
  debug::set_level(debug::Level::Error);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  LOG_ERROR("HTTP Server Demo Started");
  //   Runtime::block_on(run_server());
  Runtime::spawn(run_server());
  //  Keep the main thread alive
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }
  return 0;
}
