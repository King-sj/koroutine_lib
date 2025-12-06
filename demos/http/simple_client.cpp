#include <iostream>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_client() {
  Client cli("http://localhost:8080");

  std::cout << "GET /hi" << std::endl;
  auto res = co_await cli.Get("/hi");
  if (res) {
    std::cout << "Status: " << res->status << std::endl;
    std::cout << "Body: " << res->body << std::endl;
  } else {
    std::cout << "Error: " << to_string(res.error()) << std::endl;
  }

  std::cout << "\nPOST /echo" << std::endl;
  auto res_post = co_await cli.Post("/echo", "Hello Koroutine!", "text/plain");
  if (res_post) {
    std::cout << "Status: " << res_post->status << std::endl;
    std::cout << "Body: " << res_post->body << std::endl;
  }
}

int main() {
  Runtime::block_on(run_client());
  return 0;
}
