#include <iostream>

#include "koroutine/koroutine.h"

using namespace koroutine;
// 异步(文件) io 读取
Task<std::string> read_file(const std::string& filename) {
  // 模拟异步文件读取操作
  co_await sleep_for(1000);  // 假设读取文件需要 100 毫秒
  co_return "File content of " + filename;
}

int main() {
  std::cout << "=== Koroutine 库 Demo ===" << std::endl << std::endl;
  auto file_task = read_file("example.txt");
  std::cout << "Reading file asynchronously..." << std::endl;
  std::string content = file_task.get_result();
  std::cout << "File read complete: " << content << std::endl;
  return 0;
}
