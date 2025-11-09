#pragma once

// Lightweight, header-only logging for koroutine
// - Multiple levels (Error, Warn, Info, Debug, Trace). Trace = most verbose
// - Runtime control of level and detail flags
// - Zero-cost in release builds (when KOROUTINE_DEBUG is not defined)
// - Variadic-style template logger that streams arbitrary types with operator<<

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace koroutine {
namespace debug {

// Visible log levels. Higher value -> more verbose
enum class Level : int {
  None = 0,
  Error = 1,
  Warn = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
};

// Detail flags control which extra fields are printed
enum class Detail : unsigned {
  None = 0,
  Timestamp = 1u << 0,
  ThreadId = 1u << 1,
  FileLine = 1u << 2,
  Function = 1u << 3,
  Level = 1u << 4,
};

inline Detail operator|(Detail a, Detail b) {
  return static_cast<Detail>(static_cast<unsigned>(a) |
                             static_cast<unsigned>(b));
}
inline Detail operator&(Detail a, Detail b) {
  return static_cast<Detail>(static_cast<unsigned>(a) &
                             static_cast<unsigned>(b));
}

#ifdef KOROUTINE_DEBUG

// Runtime-configurable state (only active in KOROUTINE_DEBUG builds)
inline std::atomic<Level> g_level{Level::Info};
inline std::atomic<unsigned> g_detail_flags{
    static_cast<unsigned>(Detail::Level | Detail::Timestamp)};
inline std::mutex& g_cout_mutex() {
  static std::mutex m;
  return m;
}
inline std::ostream* g_out = &std::clog;

inline void set_level(Level l) { g_level.store(l, std::memory_order_relaxed); }
inline Level get_level() { return g_level.load(std::memory_order_relaxed); }

inline void set_detail_flags(Detail f) {
  g_detail_flags.store(static_cast<unsigned>(f));
}
inline unsigned get_detail_flags() { return g_detail_flags.load(); }

inline void set_output_stream(std::ostream& out) { g_out = &out; }

// Build header (timestamp, level, thread id, file/line, function)
inline std::string build_header(Level lvl, const char* file, int line,
                                const char* func) {
  std::ostringstream ss;
  unsigned flags = get_detail_flags();
  if (flags & static_cast<unsigned>(Detail::Timestamp)) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    std::tm tm;
    // localtime is not MT-safe; protect with mutex to be safe
    {
      std::lock_guard<std::mutex> lk(g_cout_mutex());
      tm = *std::localtime(&tt);
    }
    ss << std::put_time(&tm, "%F %T") << " ";
  }
  if (flags & static_cast<unsigned>(Detail::Level)) {
    switch (lvl) {
      case Level::Error:
        ss << "[ERROR] ";
        break;
      case Level::Warn:
        ss << "[WARN]  ";
        break;
      case Level::Info:
        ss << "[INFO]  ";
        break;
      case Level::Debug:
        ss << "[DEBUG] ";
        break;
      case Level::Trace:
        ss << "[TRACE] ";
        break;
      default:
        break;
    }
  }
  if (flags & static_cast<unsigned>(Detail::ThreadId)) {
    ss << "[T:" << std::this_thread::get_id() << "] ";
  }
  if ((flags & static_cast<unsigned>(Detail::FileLine)) && file) {
    ss << file << ":" << line << " ";
  }
  if ((flags & static_cast<unsigned>(Detail::Function)) && func) {
    ss << func << "() ";
  }
  return ss.str();
}

// Core log implementation: accept 0 or more streamable args
inline void emit(Level lvl, const std::string& header,
                 const std::string& body) {
  std::lock_guard<std::mutex> lk(g_cout_mutex());
  if (g_out) {
    (*g_out) << header << body << std::endl;
  }
}

inline void emit(Level lvl, const std::string& header) {
  emit(lvl, header, std::string());
}

template <typename... Args>
inline void log_impl(Level lvl, const char* file, int line, const char* func,
                     Args&&... args) {
  if (static_cast<int>(get_level()) < static_cast<int>(lvl)) return;
  std::ostringstream ss;
  // Stream all args into the message
  using expander = int[];
  (void)expander{0, ((ss << std::forward<Args>(args)), 0)...};
  auto header = build_header(lvl, file, line, func);
  emit(lvl, header, ss.str());
}

// Overload for zero args
inline void log_impl(Level lvl, const char* file, int line, const char* func) {
  if (static_cast<int>(get_level()) < static_cast<int>(lvl)) return;
  auto header = build_header(lvl, file, line, func);
  emit(lvl, header, std::string());
}

#else  // KOROUTINE_DEBUG

// In release builds make everything a no-op with zero runtime cost
inline void set_level(Level) {}
inline Level get_level() { return Level::None; }
inline void set_detail_flags(Detail) {}
inline unsigned get_detail_flags() { return 0u; }
inline void set_output_stream(std::ostream&) {}

template <typename... Args>
inline void log_impl(Level, const char*, int, const char*, Args&&...) {}
inline void log_impl(Level, const char*, int, const char*) {}

#endif  // KOROUTINE_DEBUG

}  // namespace debug
}  // namespace koroutine

// Convenience macros that capture file/line/function and support variadic-like
// args
#ifdef KOROUTINE_DEBUG
#define KORO_LOG_LEVEL(level, ...)                                  \
  ::koroutine::debug::log_impl(level, __FILE__, __LINE__, __func__, \
                               ##__VA_ARGS__)
#else
#define KORO_LOG_LEVEL(level, ...) ((void)0)
#endif

#ifdef KOROUTINE_DEBUG
#define LOG_TRACE(...) \
  KORO_LOG_LEVEL(::koroutine::debug::Level::Trace, ##__VA_ARGS__)
#define LOG_DEBUG(...) \
  KORO_LOG_LEVEL(::koroutine::debug::Level::Debug, ##__VA_ARGS__)
#define LOG_INFO(...) \
  KORO_LOG_LEVEL(::koroutine::debug::Level::Info, ##__VA_ARGS__)
#define LOG_WARN(...) \
  KORO_LOG_LEVEL(::koroutine::debug::Level::Warn, ##__VA_ARGS__)
#define LOG_ERROR(...) \
  KORO_LOG_LEVEL(::koroutine::debug::Level::Error, ##__VA_ARGS__)
#else
#define LOG_TRACE(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#endif
