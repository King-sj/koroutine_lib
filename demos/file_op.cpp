#define KOROUTINE_DEBUG
#include <thread>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/debug.h"
#include "koroutine/koroutine.h"
using namespace koroutine;
using namespace koroutine::async_io;

Task<void> creat_write_file(const std::string& path,
                            const std::string& content) {
  LOG_DEBUG("Creating and writing to file: ", path);
  std::shared_ptr<IOEngine> engine = IOEngine::create();
  LOG_DEBUG("IOEngine created");
  auto file =
      co_await AsyncFile::open(engine, path, std::ios::out | std::ios::trunc);
  LOG_DEBUG("File opened: ", path);
  co_await file->write(content.data(), content.size());
  LOG_DEBUG("Content written to file: ", path);
  co_await file->flush();
  LOG_DEBUG("File flushed: ", path);
  co_await file->close();
  LOG_DEBUG("File closed: ", path);
}
Task<void> read_file(const std::string& path) {
  LOG_DEBUG("Reading file: ", path);
  std::shared_ptr<IOEngine> engine = IOEngine::create();
  LOG_DEBUG("IOEngine created");
  auto file = co_await AsyncFile::open(engine, path, std::ios::in);
  LOG_DEBUG("File opened: ", path);
  char buffer[1024];
  LOG_DEBUG("Reading content from file: ", path);
  size_t n = co_await file->read(buffer, sizeof(buffer) - 1);
  buffer[n] = '\0';
  LOG_DEBUG("Content read from file: ", path);
  LOG_INFO("Read content: ", buffer);
  co_await file->close();
  LOG_DEBUG("File closed: ", path);
}

int main() {
  const std::string file_path = "test_async_file.txt";
  const std::string file_content = "Hello, koroutine async file IO!\n";

  auto write_task = creat_write_file(file_path, file_content);
  write_task.start();

  auto read_task = read_file(file_path);
  read_task.start();

  // 运行IO引擎事件循环
  std::shared_ptr<IOEngine> engine = IOEngine::create();
  std::thread io_thread([&engine]() {
    LOG_DEBUG("Starting IO engine event loop");
    engine->run();
    LOG_DEBUG("IO engine event loop exited");
  });
  Runtime::join_all(std::move(write_task), std::move(read_task));
  //   等待任务完成
  io_thread.join();
  return 0;
};