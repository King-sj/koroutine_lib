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

TEST_F(SocketTest, EndpointCheck) {
  auto test_logic = [&]() -> Task<void> {
    auto server = co_await AsyncServerSocket::bind(9999);
    auto local_ep = server->local_endpoint();
    EXPECT_EQ(local_ep.port(), 9999);

    auto client_task = [&]() -> Task<int> {
      auto socket = co_await AsyncSocket::connect("127.0.0.1", 9999);
      auto remote = socket->remote_endpoint();
      EXPECT_EQ(remote.port(), 9999);
      EXPECT_EQ(remote.address().to_string(), "127.0.0.1");
      co_return 0;
    };

    auto server_task =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();
      auto remote = client->remote_endpoint();
      // Note: remote address might be 127.0.0.1 or ::ffff:127.0.0.1 depending
      // on OS/config But since we connected to 127.0.0.1 (IPv4), it should be
      // IPv4.
      EXPECT_EQ(remote.address().to_string(), "127.0.0.1");
      co_return 0;
    };

    co_await koroutine::when_all(server_task(server), client_task());
  };
  Runtime::block_on(test_logic());
}

TEST_F(SocketTest, SocketOptions) {
  auto test_logic = [&]() -> Task<void> {
    // Test on Server Socket
    // Bind to port 0 to let OS choose a free port
    auto server = co_await AsyncServerSocket::bind(0);

    // Test ReuseAddress
    ReuseAddress reuse_addr(true);
    server->set_option(reuse_addr);

    ReuseAddress reuse_addr_get;
    server->get_option(reuse_addr_get);
    EXPECT_TRUE(reuse_addr_get.value());

    // Test ReceiveBufferSize
    int buf_size = 64 * 1024;
    ReceiveBufferSize rcv_buf(buf_size);
    server->set_option(rcv_buf);

    ReceiveBufferSize rcv_buf_get;
    server->get_option(rcv_buf_get);
    // Note: Linux kernel doubles the value set for bookkeeping overhead
    // So we check if it's at least what we set.
    EXPECT_GE(rcv_buf_get.value(), buf_size);

    // Test on Client Socket
    uint16_t port = server->local_endpoint().port();
    auto client_task = [&]() -> Task<int> {
      auto socket = co_await AsyncSocket::connect("127.0.0.1", port);

      // Test TcpNoDelay
      TcpNoDelay no_delay(true);
      socket->set_option(no_delay);

      TcpNoDelay no_delay_get;
      socket->get_option(no_delay_get);
      EXPECT_TRUE(no_delay_get.value());

      co_return 0;
    };

    auto server_task =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();
      // We can also test options on accepted socket
      TcpNoDelay no_delay_get;
      client->get_option(no_delay_get);

      // Set to false and check
      TcpNoDelay no_delay(false);
      client->set_option(no_delay);
      client->get_option(no_delay_get);
      EXPECT_FALSE(no_delay_get.value());

      co_return 0;
    };

    co_await koroutine::when_all(server_task(server), client_task());
  };
  Runtime::block_on(test_logic());
}

TEST_F(SocketTest, DNSResolution) {
  auto test_logic = [&]() -> Task<void> {
    auto server = co_await AsyncServerSocket::bind(0);
    uint16_t port = server->local_endpoint().port();

    auto client_task = [&]() -> Task<int> {
      // "localhost" should be resolved to 127.0.0.1 or ::1
      // This verifies that Resolver::resolve is called and works
      auto socket = co_await AsyncSocket::connect("localhost", port);
      co_await socket->write("ping", 4);
      co_return 0;
    };

    auto server_task =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();
      char buf[1024];
      size_t n = co_await client->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "ping");
      co_return 0;
    };

    co_await koroutine::when_all(server_task(server), client_task());
  };
  Runtime::block_on(test_logic());
}