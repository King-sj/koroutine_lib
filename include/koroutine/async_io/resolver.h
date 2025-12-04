#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "koroutine/async_io/endpoint.h"
#include "koroutine/task.hpp"

namespace koroutine::async_io {

class Resolver {
 public:
  // Resolve host and port to a list of Endpoints
  static Task<std::expected<std::vector<Endpoint>, int>> resolve(
      const std::string& host, uint16_t port);

  // Resolve host and service name (e.g. "http") to a list of Endpoints
  static Task<std::expected<std::vector<Endpoint>, int>> resolve(
      const std::string& host, const std::string& service);
};

}  // namespace koroutine::async_io
