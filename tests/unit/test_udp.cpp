#include <gtest/gtest.h>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

class UDPTest : public ::testing::Test {};

TEST_F(UDPTest, SendReceiveIPv4) {
  auto test_logic = [&]() -> Task<void> {
    auto server = co_await AsyncUDPSocket::create(IPAddress::Type::IPv4);
    co_await server->bind(Endpoint("127.0.0.1", 9997));

    auto client = co_await AsyncUDPSocket::create(IPAddress::Type::IPv4);

    std::string msg = "Hello UDP";
    Endpoint server_ep("127.0.0.1", 9997);

    co_await client->send_to(msg.data(), msg.size(), server_ep);

    char buf[1024];
    auto [n, sender_ep] = co_await server->recv_from(buf, sizeof(buf));

    std::string received(buf, n);
    EXPECT_EQ(received, msg);
    EXPECT_EQ(sender_ep.address().to_string(), "127.0.0.1");
  };
  Runtime::block_on(test_logic());
}

TEST_F(UDPTest, SendReceiveIPv6) {
  auto test_logic = [&]() -> Task<void> {
    auto server = co_await AsyncUDPSocket::create(IPAddress::Type::IPv6);
    co_await server->bind(Endpoint("::1", 9996));

    auto client = co_await AsyncUDPSocket::create(IPAddress::Type::IPv6);

    std::string msg = "Hello IPv6 UDP";
    Endpoint server_ep("::1", 9996);

    co_await client->send_to(msg.data(), msg.size(), server_ep);

    char buf[1024];
    auto [n, sender_ep] = co_await server->recv_from(buf, sizeof(buf));

    std::string received(buf, n);
    EXPECT_EQ(received, msg);
    EXPECT_EQ(sender_ep.address().to_string(), "::1");
  };
  Runtime::block_on(test_logic());
}
