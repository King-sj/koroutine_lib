#pragma once

#include <type_traits>

#include "../task.hpp"

namespace koroutine::details {

// Concept to constrain that T is derived from TaskBase
template <typename T>
concept TaskType = requires {
  typename T::promise_type;
  requires std::derived_from<
      T, TaskBase<typename T::promise_type::result_type, T>>;
};

}  // namespace koroutine::details