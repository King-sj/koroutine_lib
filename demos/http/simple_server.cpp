#include <iostream>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server() {
  auto svr = std::make_shared<Server>();

  svr->Get("/hi", [](const Request& req, Response& res) -> Task<void> {
    res.set_content("Hello World!", "text/plain");
    co_return;
  });

  svr->Get(R"(/users/(\d+))",
           [](const Request& req, Response& res) -> Task<void> {
             auto user_id = req.matches[1];
             res.set_content("User ID: " + std::string(user_id), "text/plain");
             co_return;
           });

  svr->Post("/echo", [](const Request& req, Response& res) -> Task<void> {
    res.set_content(req.body, "text/plain");
    co_return;
  });

  std::cout << "Server listening on http://0.0.0.0:8080" << std::endl;
  bool ret = co_await svr->listen_async("0.0.0.0", 8080);

  if (!ret) {
    std::cerr << "Failed to listen" << std::endl;
  }
}

int main() {
  // debug::set_level(debug::Level::Info);
  Runtime::block_on(run_server());
  return 0;
}
