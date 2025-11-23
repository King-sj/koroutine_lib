#include <gtest/gtest.h>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

class SocketTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup code if needed
  }
};

TEST_F(SocketTest, ConnectAndCommunicate) {
  bool server_done = false;
  bool client_done = false;

  auto test_logic = [&]() -> Task<void> {
    // 1. Start server and bind
    auto server = co_await AsyncServerSocket::bind(9998);

    // 2. Define server and client interactions
    auto server_handler =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();
      char buf[1024];
      size_t n = co_await client->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "ping");
      co_await client->write("pong", 4);
      server_done = true;
      co_return 0;
    };

    auto client_handler = [&]() -> Task<int> {
      auto socket = co_await AsyncSocket::connect("127.0.0.1", 9998);
      co_await socket->write("ping", 4);
      char buf[1024];
      size_t n = co_await socket->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "pong");
      client_done = true;
      co_return 0;
    };

    // 3. Run them concurrently
    // We need to share ownership of server socket or move it.
    // Since server_handler takes it, we can move it.
    // But we need to make sure server_handler keeps it alive.
    // Using shared_ptr for convenience in lambda capture/args
    std::shared_ptr<AsyncServerSocket> shared_server = std::move(server);

    // Use a helper to run both
    co_await koroutine::when_all(server_handler(shared_server),
                                 client_handler());
  };

  Runtime::block_on(test_logic());

  EXPECT_TRUE(server_done);
  EXPECT_TRUE(client_done);
}
