#include <cstring>
#include <string>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

Task<void> run_echo() {
  auto in = get_stdin();
  auto out = get_stdout();

  const char* msg = "Please type something (Ctrl+D to exit):\n";
  co_await out->write(msg, std::strlen(msg));

  char buf[128];
  while (true) {
    size_t n = co_await in->read(buf, sizeof(buf));
    if (n == 0) {
      break;
    }
    co_await out->write("Echo: ", 6);
    co_await out->write(buf, n);
  }

  const char* bye = "Goodbye!\n";
  co_await out->write(bye, std::strlen(bye));
}

int main() {
  Runtime::block_on(run_echo());
  return 0;
}
