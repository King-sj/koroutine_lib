#include <iostream>
#include <memory>

#include "koroutine/koroutine.h"
#include "koroutine/task.hpp"

using namespace koroutine;

class TestClass {
 public:
  int value;
  TestClass(int v) : value(v) {
    std::cout << "TestClass constructed: " << value << std::endl;
  }
};

Task<std::unique_ptr<TestClass>> create_object() {
  std::cout << "create_object: START" << std::endl;
  auto ptr = std::make_unique<TestClass>(42);
  std::cout << "create_object: ptr address = " << ptr.get() << std::endl;
  co_return std::move(ptr);
}

Task<void> test_task() {
  std::cout << "test_task: calling create_object" << std::endl;
  auto obj = co_await create_object();
  std::cout << "test_task: got object, ptr = " << obj.get() << std::endl;
  if (obj) {
    std::cout << "test_task: object value = " << obj->value << std::endl;
  } else {
    std::cout << "test_task: object is NULL!" << std::endl;
  }
}

int main() {
  std::cout << "main: Starting" << std::endl;
  auto task = test_task();
  Runtime::join_all(std::move(task));
  std::cout << "main: Done" << std::endl;
  return 0;
}
