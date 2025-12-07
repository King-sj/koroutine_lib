#pragma once
// 作用是: 确保包含此头文件的源文件支持C++20协程特性
//#define __cpp_lib_coroutine
#include <coroutine>

#if defined(_WIN32) || defined(_WIN64)
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <deque>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#endif