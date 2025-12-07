#define KOROUTINE_DEBUG
#include <gtest/gtest.h>

#include <memory>
#include <random>

#include "koroutine/async_io/httplib.h"
#include "koroutine/debug.h"
#include "koroutine/koroutine.h"
#include "koroutine/when_all.hpp"
using namespace koroutine;
using namespace httplib;

class HttpTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup code if needed
    debug::set_level(debug::Level::Error);
    debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                            debug::Detail::ThreadId | debug::Detail::FileLine);
  }

  void TearDown() override {
    // Teardown code if needed
  }

  // Helper to generate random string
  std::string random_string(size_t length) {
    auto randchar = []() -> char {
      const char charset[] =
          "0123456789"
          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyz";
      const size_t max_index = (sizeof(charset) - 1);
      return charset[rand() % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
  }
};

TEST_F(HttpTest, BasicGetPost) {
  auto test_task = []() -> Task<void> {
    std::cout << "Starting BasicGetPost" << std::endl;
    auto svr = std::make_shared<Server>();
    int port = 8082;

    svr->Get("/hi", [](const Request&, Response& res) -> Task<void> {
      std::cout << "Handling /hi" << std::endl;
      res.set_content("Hello World!", "text/plain");
      co_return;
    });

    svr->Post("/echo", [](const Request& req, Response& res) -> Task<void> {
      std::cout << "Handling /echo" << std::endl;
      res.set_content(req.body, "text/plain");
      co_return;
    });

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<int> {
      try {
        bool ret = co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
      co_return 0;
    };

    auto client_task = [](std::shared_ptr<Server> svr, int port) -> Task<int> {
        co_await koroutine::SleepAwaiter(100);

        EXPECT_EQ(svr->bind_port(), port);

        Client cli("http://127.0.0.1:" + std::to_string(port));

        // Test GET
        auto res = co_await cli.Get("/hi");
        if (!res) {
            std::cout << "GET /hi failed. Error: " << res.error() << std::endl;
        }
        EXPECT_TRUE(res);
        if (res) {
          EXPECT_EQ(res->status, 200);
          EXPECT_EQ(res->body, "Hello World!");
        }

        // Test POST
        auto res_post = co_await cli.Post("/echo", "test body", "text/plain");
        if (!res_post) {
            std::cout << "POST /echo failed. Error: " << res_post.error() << std::endl;
        }
        EXPECT_TRUE(res_post);
        if (res_post) {
          EXPECT_EQ(res_post->status, 200);
          EXPECT_EQ(res_post->body, "test body");
        }

        svr->stop();
        co_return 0;
    };

    co_await when_all(server_task(svr, port), client_task(svr, port));
  };

  Runtime::block_on(test_task());
}

TEST_F(HttpTest, LargeHeaders) {
  auto test_task = [this]() -> Task<void> {
    auto svr = std::make_shared<Server>();
    int port = 8083;

    svr->Get("/large-header", [](const Request&, Response& res) -> Task<void> {
      res.set_content("ok", "text/plain");
      co_return;
    });

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<void> {
      try {
        co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
    };
    koroutine::Runtime::spawn(server_task(svr, port));

    co_await koroutine::SleepAwaiter(100);
    EXPECT_EQ(svr->bind_port(), port);

    {
      Client cli("http://127.0.0.1:8083");

      Headers headers;
      for (int i = 0; i < 50; ++i) {
        headers.emplace("X-Custom-Header-" + std::to_string(i),
                        random_string(100));
      }

      auto res = co_await cli.Get("/large-header", headers);
      EXPECT_TRUE(res);
      if (res) {
        EXPECT_EQ(res->status, 200);
      }
    }

    std::cout << "Calling svr->stop()" << std::endl;
    svr->stop();
    std::cout << "svr->stop() returned" << std::endl;
    std::cout << "test_task finishing" << std::endl;
  };

  Runtime::block_on(test_task());
  std::cout << "Runtime::block_on returned" << std::endl;
}

TEST_F(HttpTest, NotFound) {
  auto test_task = []() -> Task<void> {
    auto svr = std::make_shared<Server>();
    int port = 8084;

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<void> {
      try {
        co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
    };
    koroutine::Runtime::spawn(server_task(svr, port));

    co_await koroutine::SleepAwaiter(100);
    EXPECT_EQ(svr->bind_port(), port);

    Client cli("http://127.0.0.1:8084");

    auto res = co_await cli.Get("/not-found");
    EXPECT_TRUE(res);
    if (res) {
      EXPECT_EQ(res->status, 404);
    }

    svr->stop();
  };

  Runtime::block_on(test_task());
}

TEST_F(HttpTest, ConcurrentRequests) {
  auto test_task = []() -> Task<void> {
    auto svr = std::make_shared<Server>();
    int port = 8085;

    svr->Get("/sleep", [](const Request&, Response& res) -> Task<void> {
      co_await koroutine::SleepAwaiter(50);
      res.set_content("woke up", "text/plain");
      co_return;
    });

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<void> {
      try {
        co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
    };
    koroutine::Runtime::spawn(server_task(svr, port));

    co_await koroutine::SleepAwaiter(100);
    EXPECT_EQ(svr->bind_port(), port);

    Client cli1("http://127.0.0.1:8085");
    Client cli2("http://127.0.0.1:8085");
    Client cli3("http://127.0.0.1:8085");

    // auto req1 = cli1.Get("/sleep");
    // auto req2 = cli2.Get("/sleep");
    // auto req3 = cli3.Get("/sleep");

    // add some different delays
    auto req1 = []() -> Task<httplib::Result> {
      co_await koroutine::SleepAwaiter(10);
      co_return co_await Client("http://127.0.0.1:8085").Get("/sleep");
    }();
    auto req2 = []() -> Task<httplib::Result> {
      co_await koroutine::SleepAwaiter(20);
      co_return co_await Client("http://127.0.0.1:8085").Get("/sleep");
    }();
    auto req3 = []() -> Task<httplib::Result> {
      co_await koroutine::SleepAwaiter(30);
      co_return co_await Client("http://127.0.0.1:8085").Get("/sleep");
    }();

    auto [r1, r2, r3] =
        co_await when_all(std::move(req1), std::move(req2), std::move(req3));

    EXPECT_TRUE(r1) << "Error: " << (int)r1.error();
    EXPECT_TRUE(r2) << "Error: " << (int)r2.error();
    EXPECT_TRUE(r3) << "Error: " << (int)r3.error();

    if (r1) EXPECT_EQ(r1->status, 200);
    if (r2) EXPECT_EQ(r2->status, 200);
    if (r3) EXPECT_EQ(r3->status, 200);

    svr->stop();
  };

  Runtime::block_on(test_task());
}

TEST_F(HttpTest, LargeBody) {
  auto test_task = [this]() -> Task<void> {
    auto svr = std::make_shared<Server>();
    int port = 8086;

    svr->Post("/large", [](const Request& req, Response& res) -> Task<void> {
      res.set_content(req.body, "text/plain");
      co_return;
    });

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<void> {
      try {
        co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
    };
    koroutine::Runtime::spawn(server_task(svr, port));

    co_await koroutine::SleepAwaiter(100);
    EXPECT_EQ(svr->bind_port(), port);

    Client cli("http://127.0.0.1:8086");

    std::string body = random_string(1024 * 1024);  // 1MB
    auto res = co_await cli.Post("/large", body, "text/plain");
    EXPECT_TRUE(res);
    if (res) {
      EXPECT_EQ(res->status, 200);
      EXPECT_EQ(res->body.size(), body.size());
      EXPECT_EQ(res->body, body);
    }

    svr->stop();
  };

  Runtime::block_on(test_task());
}

TEST_F(HttpTest, KeepAlive) {
  auto test_task = []() -> Task<void> {
    auto svr = std::make_shared<Server>();
    int port = 8087;

    svr->Get("/ping", [](const Request&, Response& res) -> Task<void> {
      res.set_content("pong", "text/plain");
      co_return;
    });

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<void> {
      try {
        co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
    };
    koroutine::Runtime::spawn(server_task(svr, port));

    co_await koroutine::SleepAwaiter(100);
    EXPECT_EQ(svr->bind_port(), port);

    Client cli("http://127.0.0.1:8087");
    cli.set_keep_alive(true);

    for (int i = 0; i < 5; ++i) {
      auto res = co_await cli.Get("/ping");
      EXPECT_TRUE(res);
      if (res) {
        EXPECT_EQ(res->status, 200);
        EXPECT_EQ(res->body, "pong");
      }
    }

    svr->stop();
  };

  Runtime::block_on(test_task());
}

TEST_F(HttpTest, MultipartFormData) {
  auto test_task = []() -> Task<void> {
    auto svr = std::make_shared<Server>();
    int port = 8088;

    svr->Post("/upload", [](const Request& req, Response& res) -> Task<void> {
      if (req.form.has_file("file1")) {
        auto file = req.form.get_file("file1");
        res.set_content(file.content, "text/plain");
      } else {
        res.status = 400;
      }
      co_return;
    });

    auto server_task = [](std::shared_ptr<Server> svr, int port) -> Task<void> {
      try {
        co_await svr->listen_async("127.0.0.1", port);
      } catch (const std::exception& e) {
        std::cout << "Server listen threw exception: " << e.what() << std::endl;
      } catch (...) {
        std::cout << "Server listen threw unknown exception" << std::endl;
      }
    };
    koroutine::Runtime::spawn(server_task(svr, port));

    co_await koroutine::SleepAwaiter(100);
    EXPECT_EQ(svr->bind_port(), port);

    Client cli("http://127.0.0.1:8088");

    UploadFormDataItems items = {
        {"file1", "hello world", "hello.txt", "text/plain"},
    };

    auto res = co_await cli.Post("/upload", items);
    EXPECT_TRUE(res);
    if (res) {
      EXPECT_EQ(res->status, 200);
      EXPECT_EQ(res->body, "hello world");
    }

    svr->stop();
  };

  Runtime::block_on(test_task());
}
