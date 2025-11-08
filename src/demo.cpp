#include <iostream>

#include "koroutine/koroutine.h"

using namespace koroutine;

// 示例1: 简单的Task协程
Task<int> calculate_answer() {
  std::cout << "计算中..." << std::endl;
  co_return 42;
}

// 示例2: 字符串Task
Task<std::string> get_greeting(const std::string& name) {
  co_return "你好, " + name + "!";
}

// 示例3: 数字生成器
Generator<int> fibonacci(int n) {
  int a = 0, b = 1;
  for (int i = 0; i < n; ++i) {
    co_yield a;
    int temp = a;
    a = b;
    b = temp + b;
  }
}

// 示例4: 范围生成器
Generator<int> range(int start, int end, int step = 1) {
  for (int i = start; i < end; i += step) {
    co_yield i;
  }
}

int main() {
  std::cout << "=== Koroutine 库 Demo ===" << std::endl << std::endl;

  // 演示Task<int>
  std::cout << "1. Task<int> 示例:" << std::endl;
  auto task1 = calculate_answer();
  task1.resume();
  std::cout << "答案是: " << task1.get_result() << std::endl << std::endl;

  // 演示Task<std::string>
  std::cout << "2. Task<std::string> 示例:" << std::endl;
  auto task2 = get_greeting("协程世界");
  task2.resume();
  std::cout << task2.get_result() << std::endl << std::endl;

  // 演示Generator - 斐波那契数列
  std::cout << "3. Generator - 斐波那契数列(前10个):" << std::endl;
  auto fib = fibonacci(10);
  while (fib.next()) {
    std::cout << fib.value() << " ";
  }
  std::cout << std::endl << std::endl;

  // 演示Generator - 范围生成器
  std::cout << "4. Generator - 范围生成器(0到20，步长2):" << std::endl;
  auto gen = range(0, 20, 2);
  while (gen.next()) {
    std::cout << gen.value() << " ";
  }
  std::cout << std::endl << std::endl;

  // 演示Task<void>
  std::cout << "5. Task<void> 示例:" << std::endl;
  auto void_task = []() -> Task<void> {
    std::cout << "执行无返回值的协程" << std::endl;
    co_return;
  }();
  void_task.resume();
  std::cout << "协程执行完成" << std::endl << std::endl;

  std::cout << "=== Demo 完成 ===" << std::endl;

  return 0;
}
