#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"
#include "koroutine/sync/async_condition_variable.h"
#include "koroutine/sync/async_mutex.h"

using namespace koroutine;
using namespace koroutine::async_io;

// Helper function to ensure file cleanup
class TempFile {
 public:
  explicit TempFile(const std::string& name) : filename(name) {}
  ~TempFile() { std::remove(filename.c_str()); }
  const std::string& name() const { return filename; }

 private:
  std::string filename;
};

// 测试基本的文件打开和关闭
TEST(AsyncIOTest, BasicFileOpenClose) {
  TempFile temp("test_open_close.txt");
  auto engine = IOEngine::create();

  auto test_task = [&]() -> Task<void> {
    auto file = co_await AsyncFile::open(engine, temp.name(),
                                         std::ios::out | std::ios::trunc);
    EXPECT_NE(file, nullptr);
    co_await file->close();
    engine->stop();
  }();

  std::thread io_thread([&engine]() { engine->run(); });

  Runtime::block_on(std::move(test_task));

  io_thread.join();
}

// 测试文件写入
TEST(AsyncIOTest, FileWrite) {
  TempFile temp("test_write.txt");
  const std::string test_content = "Hello, AsyncIO!";
  auto engine = IOEngine::create();

  auto test_task = [&]() -> Task<void> {
    auto file = co_await AsyncFile::open(engine, temp.name(),
                                         std::ios::out | std::ios::trunc);
    EXPECT_NE(file, nullptr);

    co_await file->write(test_content.data(), test_content.size());
    co_await file->flush();
    co_await file->close();
    engine->stop();
  }();

  std::thread io_thread([&engine]() { engine->run(); });

  Runtime::block_on(std::move(test_task));

  io_thread.join();

  // 验证文件内容
  std::ifstream verify(temp.name());
  std::string content((std::istreambuf_iterator<char>(verify)),
                      std::istreambuf_iterator<char>());
  EXPECT_EQ(content, test_content);
}

// 测试文件读取
TEST(AsyncIOTest, FileRead) {
  TempFile temp("test_read.txt");
  const std::string test_content = "Test content for reading";

  // 先创建文件并写入内容
  {
    std::ofstream out(temp.name());
    out << test_content;
  }

  auto engine = IOEngine::create();

  auto test_task = [&]() -> Task<std::string> {
    auto file = co_await AsyncFile::open(engine, temp.name(), std::ios::in);
    EXPECT_NE(file, nullptr);

    char buffer[256];
    size_t n = co_await file->read(buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    co_await file->close();
    engine->stop();

    co_return std::string(buffer);
  }();

  std::thread io_thread([&engine]() { engine->run(); });

  auto result = Runtime::block_on(std::move(test_task));

  io_thread.join();

  EXPECT_EQ(result, test_content);
}

// 测试写入后读取
TEST(AsyncIOTest, WriteAndRead) {
  TempFile temp("test_write_read.txt");
  const std::string test_content = "Write and read test!";
  auto engine = IOEngine::create();

  auto test_task = [&]() -> Task<std::string> {
    // 写入
    {
      auto file = co_await AsyncFile::open(engine, temp.name(),
                                           std::ios::out | std::ios::trunc);
      EXPECT_NE(file, nullptr);
      co_await file->write(test_content.data(), test_content.size());
      co_await file->flush();
      co_await file->close();
    }

    // 读取
    {
      auto file = co_await AsyncFile::open(engine, temp.name(), std::ios::in);
      EXPECT_NE(file, nullptr);

      char buffer[256];
      size_t n = co_await file->read(buffer, sizeof(buffer) - 1);
      buffer[n] = '\0';
      co_await file->close();
      engine->stop();

      co_return std::string(buffer);
    }
  }();

  std::thread io_thread([&engine]() { engine->run(); });

  auto result = Runtime::block_on(std::move(test_task));

  io_thread.join();

  EXPECT_EQ(result, test_content);
}

// 测试带同步的文件操作（类似demo中的场景）
TEST(AsyncIOTest, SynchronizedWriteAndRead) {
  TempFile temp("test_sync_write_read.txt");
  const std::string test_content = "Synchronized content";
  auto engine = IOEngine::create();

  AsyncMutex io_mutex;
  AsyncConditionVariable completed;
  bool write_completed = false;

  auto write_task = [&]() -> Task<void> {
    co_await io_mutex.lock();
    auto file = co_await AsyncFile::open(engine, temp.name(),
                                         std::ios::out | std::ios::trunc);
    co_await file->write(test_content.data(), test_content.size());
    co_await file->flush();
    co_await file->close();

    write_completed = true;
    io_mutex.unlock();
    completed.notify_all();
  }();

  auto read_task = [&]() -> Task<std::string> {
    co_await io_mutex.lock();
    while (!write_completed) {
      co_await completed.wait(io_mutex);
    }

    auto file = co_await AsyncFile::open(engine, temp.name(), std::ios::in);
    char buffer[256];
    size_t n = co_await file->read(buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    co_await file->close();
    io_mutex.unlock();
    engine->stop();

    co_return std::string(buffer);
  }();

  std::thread io_thread([&engine]() { engine->run(); });

  Runtime::join_all(std::move(write_task), std::move(read_task));

  io_thread.join();

  EXPECT_TRUE(write_completed);
}
