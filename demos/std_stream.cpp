#include <cstring>
#include <string>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

Task<void> run_echo() {
  // auto in = get_stdin();
  // auto out = get_stdout();

  co_await (cout << "Please type something (Ctrl+D to exit):\n");

  std::string buf;
  while (true) {
    // Read a word (space delimited)
    co_await (cin >> buf);
    if (buf.empty()) {
      break;
    }
    co_await (cout << "Echo: " << buf << "\n");
  }

  co_await (cout << "Goodbye!\n");
}

int main() {
  Runtime::block_on(run_echo());
  return 0;
}
