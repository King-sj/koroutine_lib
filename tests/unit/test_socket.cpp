#include <gtest/gtest.h>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/async_io/resolver.h"
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

TEST_F(SocketTest, DNSResolutionComplex) {
  auto test_logic = [&]() -> Task<void> {
    // 1. Test resolving invalid host
    auto result =
        co_await Resolver::resolve("invalid.host.name.that.does.not.exist", 80);
    EXPECT_FALSE(result.has_value());

    // 2. Test resolving service name
    // "http" usually maps to 80
    auto result_http = co_await Resolver::resolve("localhost", "http");
    // We expect success or at least a valid return (even if empty if localhost
    // not defined, but localhost usually is)
    if (result_http) {
      bool found_80 = false;
      for (const auto& ep : result_http.value()) {
        if (ep.port() == 80) {
          found_80 = true;
          break;
        }
      }
      EXPECT_TRUE(found_80);
    }

    // 3. Concurrent resolutions
    auto run_resolve = [](std::string host, uint16_t port) -> Task<bool> {
      auto res = co_await Resolver::resolve(host, port);
      co_return res.has_value() && !res.value().empty();
    };

    auto [r1, r2] = co_await koroutine::when_all(
        run_resolve("localhost", 80), run_resolve("127.0.0.1", 8080));

    EXPECT_TRUE(r1);
    EXPECT_TRUE(r2);
  };
  Runtime::block_on(test_logic());
}

TEST_F(SocketTest, DNSResolutionRealWorld) {
  auto test_logic = [&]() -> Task<void> {
    // Test resolving real world domains
    // Note: This test requires internet connection

    // 1. Resolve google.com
    auto result_google = co_await Resolver::resolve("google.com", 80);
    if (result_google) {
      EXPECT_FALSE(result_google.value().empty());
      for (const auto& ep : result_google.value()) {
        // Just print for debug info, don't fail if IP is weird
        // std::cout << "google.com resolved to: " << ep.address().to_string()
        // << std::endl;
        EXPECT_EQ(ep.port(), 80);
      }
    } else {
      // If network is down, we might get an error.
      // We can warn but maybe not fail hard if it's just network issue?
      // For now let's assume test environment has network.
      // If not, this test might be flaky.
      std::cerr << "Failed to resolve google.com: " << result_google.error()
                << std::endl;
    }

    // 2. Resolve baidu.com
    auto result_baidu = co_await Resolver::resolve("baidu.com", 443);
    if (result_baidu) {
      EXPECT_FALSE(result_baidu.value().empty());
      for (const auto& ep : result_baidu.value()) {
        EXPECT_EQ(ep.port(), 443);
      }
      // print ip addresses for debug
      for (const auto& ep : result_baidu.value()) {
        std::cout << "baidu.com resolved to: " << ep.address().to_string()
                  << std::endl;
      }
    } else {
      std::cerr << "Failed to resolve baidu.com: " << result_baidu.error()
                << std::endl;
    }
  };
  Runtime::block_on(test_logic());
}