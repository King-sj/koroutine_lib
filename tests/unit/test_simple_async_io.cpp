#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace koroutine::async_io;

class TempFile {
 public:
  explicit TempFile(const std::string& name) : filename(name) {}
  ~TempFile() { std::remove(filename.c_str()); }
  const std::string& name() const { return filename; }

 private:
  std::string filename;
};

TEST(SimpleAsyncIOTest, ImplicitEngineFileWriteRead) {
  TempFile temp("test_simple_io.txt");
  const std::string test_content = "Simple Async IO Test";

  auto test_lambda = [&]() -> Task<void> {
    // Write
    {
      auto file = co_await AsyncFile::open(temp.name(),
                                           std::ios::out | std::ios::trunc);
      EXPECT_NE(file, nullptr);
      co_await file->write(test_content.data(), test_content.size());
      co_await file->close();
    }

    // Read
    {
      auto file = co_await AsyncFile::open(temp.name(), std::ios::in);
      EXPECT_NE(file, nullptr);
      char buffer[256];
      size_t n = co_await file->read(buffer, sizeof(buffer) - 1);
      buffer[n] = '\0';
      EXPECT_EQ(std::string(buffer), test_content);
      co_await file->close();
    }
  };
  auto test_task = test_lambda();

  Runtime::block_on(std::move(test_task));
}
