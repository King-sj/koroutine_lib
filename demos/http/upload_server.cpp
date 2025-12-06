#define KOROUTINE_DEBUG
#include <fstream>
#include <iostream>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

Task<void> run_server() {
  auto svr = std::make_shared<Server>();

  svr->Get("/", [](const Request&, Response& res) -> Task<void> {
    res.set_content(
        "<html><body>"
        "<form action=\"/upload\" method=\"post\" "
        "enctype=\"multipart/form-data\">"
        "  <input type=\"file\" name=\"file\">"
        "  <input type=\"submit\">"
        "</form>"
        "</body></html>",
        "text/html");
    co_return;
  });

  svr->Post("/upload", [](const Request& req, Response& res) -> Task<void> {
    if (req.form.has_file("file")) {
      auto file = req.form.get_file("file");
      std::cout << "Received file: " << file.filename << std::endl;
      std::cout << "Content type: " << file.content_type << std::endl;
      std::cout << "Size: " << file.content.size() << " bytes" << std::endl;

      // Save to disk (optional)
      // std::ofstream ofs(file.filename, std::ios::binary);
      // ofs.write(file.content.data(), file.content.size());

      res.set_content(
          "Upload successful! Size: " + std::to_string(file.content.size()),
          "text/plain");
    } else {
      res.status = 400;
      res.set_content("No file uploaded", "text/plain");
    }
    co_return;
  });

  std::cout << "Upload Server listening on http://0.0.0.0:8081" << std::endl;

  co_await svr->listen_async("0.0.0.0", 8081);
}

int main() {
  debug::set_level(debug::Level::Info);
  Runtime::block_on(run_server());
  return 0;
}
