#include <gtest/gtest.h>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

class IPv6Test : public ::testing::Test {};

TEST_F(IPv6Test, TCPConnectAndCommunicate) {
  auto test_logic = [&]() -> Task<void> {
    // 1. Start IPv6 server
    // Bind to localhost IPv6 address [::1]
    auto server = co_await AsyncServerSocket::bind(Endpoint("::1", 9995));

    auto server_handler =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();

      // Verify remote endpoint is IPv6
      auto remote = client->remote_endpoint();
      EXPECT_EQ(remote.address().type(), IPAddress::Type::IPv6);
      EXPECT_EQ(remote.address().to_string(), "::1");

      char buf[1024];
      size_t n = co_await client->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "Hello IPv6");

      co_await client->write("Ack IPv6", 8);
      co_return 0;
    };

    auto client_handler = [&]() -> Task<int> {
      // Connect to IPv6 server
      auto socket = co_await AsyncSocket::connect(Endpoint("::1", 9995));

      // Verify local endpoint is IPv6
      auto local = socket->local_endpoint();
      EXPECT_EQ(local.address().type(), IPAddress::Type::IPv6);

      co_await socket->write("Hello IPv6", 10);

      char buf[1024];
      size_t n = co_await socket->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "Ack IPv6");
      co_return 0;
    };

    std::shared_ptr<AsyncServerSocket> shared_server = std::move(server);
    co_await koroutine::when_all(server_handler(shared_server),
                                 client_handler());
  };

  Runtime::block_on(test_logic());
}

TEST_F(IPv6Test, TCPConnectHostname) {
  auto test_logic = [&]() -> Task<void> {
    // Bind to ::1
    auto server = co_await AsyncServerSocket::bind(Endpoint("::1", 9994));

    auto server_handler =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();
      co_await client->write("pong", 4);
      co_return 0;
    };

    auto client_handler = [&]() -> Task<int> {
      // Connect using hostname "localhost" which should resolve to ::1 (among
      // others) Note: This depends on /etc/hosts having ::1 localhost If it
      // resolves to 127.0.0.1 first, connection might fail if server only bound
      // to ::1 So we explicitly use ip6-localhost if available, or just skip if
      // we can't guarantee. But let's try connecting to "::1" as string, which
      // AsyncSocket::connect(host, port) handles.

      auto socket = co_await AsyncSocket::connect("::1", 9994);
      char buf[1024];
      size_t n = co_await socket->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "pong");
      co_return 0;
    };

    std::shared_ptr<AsyncServerSocket> shared_server = std::move(server);
    co_await koroutine::when_all(server_handler(shared_server),
                                 client_handler());
  };

  Runtime::block_on(test_logic());
}

TEST_F(IPv6Test, BindIPv6Any) {
  auto test_logic = [&]() -> Task<void> {
    // Bind to [::] (IPv6 ANY)
    auto server = co_await AsyncServerSocket::bind(
        Endpoint(IPAddress::any(IPAddress::Type::IPv6), 0));

    // Get the assigned port
    uint16_t port = server->local_endpoint().port();
    EXPECT_GT(port, 0);

    auto server_handler =
        [&](std::shared_ptr<AsyncServerSocket> srv) -> Task<int> {
      auto client = co_await srv->accept();
      co_await client->write("any", 3);
      co_return 0;
    };

    auto client_handler = [&]() -> Task<int> {
      // Connect to ::1 on that port
      auto socket = co_await AsyncSocket::connect(Endpoint("::1", port));
      char buf[1024];
      size_t n = co_await socket->read(buf, sizeof(buf));
      std::string msg(buf, n);
      EXPECT_EQ(msg, "any");
      co_return 0;
    };

    std::shared_ptr<AsyncServerSocket> shared_server = std::move(server);
    co_await koroutine::when_all(server_handler(shared_server),
                                 client_handler());
  };

  Runtime::block_on(test_logic());
}
