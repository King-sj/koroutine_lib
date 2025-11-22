#define KOROUTINE_DEBUG
#include <thread>

#include "koroutine/async_io/async_io.hpp"
#include "koroutine/debug.h"
#include "koroutine/koroutine.h"
#include "koroutine/sync/async_condition_variable.h"
#include "koroutine/sync/async_mutex.h"
using namespace koroutine;
using namespace koroutine::async_io;

AsyncMutex io_mutex;
AsyncConditionVariable completed;
bool write_completed = false;

Task<void> creat_write_file(const std::string& path,
                            const std::string& content) {
  LOG_DEBUG("Waiting to write file: ", path);
  co_await io_mutex.lock();
  LOG_DEBUG("Creating and writing to file: ", path);
  std::shared_ptr<IOEngine> engine = IOEngine::create();
  LOG_DEBUG("IOEngine created");
  auto file =
      co_await AsyncFile::open(engine, path, std::ios::out | std::ios::trunc);
  LOG_DEBUG("File opened: ", path, " file ptr=", (void*)file.get());
  if (!file) {
    LOG_ERROR("File is null after open!");
    co_return;
  }
  LOG_DEBUG("About to write, file ptr=", (void*)file.get());
  try {
    co_await file->write(content.data(), content.size());
    LOG_DEBUG("Content written to file: ", path);
    co_await file->flush();
    LOG_DEBUG("File flushed: ", path);
    co_await file->close();
    LOG_DEBUG("File closed: ", path);
  } catch (const std::exception& e) {
    LOG_ERROR("Exception during file operations: ", e.what());
    throw;
  }
  LOG_DEBUG("About to unlock mutex");
  write_completed = true;  // 设置完成标志
  io_mutex.unlock();
  LOG_DEBUG("Mutex unlocked, about to notify_all");
  completed.notify_all();
  LOG_DEBUG("notify_all completed");
}
Task<void> read_file(const std::string& path) {
  LOG_DEBUG("Waiting to read file: ", path);
  co_await io_mutex.lock();
  // 使用while循环等待条件，避免虚假唤醒和错过通知
  while (!write_completed) {
    co_await completed.wait(io_mutex);
  }
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
  debug::set_level(debug::Level::Trace);
  debug::set_detail_flags(debug::Detail::ThreadId | debug::Detail::Timestamp |
                          debug::Detail::FileLine | debug::Detail::Level);

  const std::string file_path = "test_async_file.txt";
  const std::string file_content = "Hello, koroutine async file IO!\n";

  auto write_task = creat_write_file(file_path, file_content);
  auto read_task = read_file(file_path);

  // 运行IO引擎事件循环
  std::shared_ptr<IOEngine> engine = IOEngine::create();
  std::thread io_thread([&engine]() {
    LOG_DEBUG("Starting IO engine event loop");
    engine->run();
    LOG_DEBUG("IO engine event loop exited");
  });
  io_thread.detach();
  Runtime::join_all(std::move(write_task), std::move(read_task));
  //   等待任务完成
  return 0;
};