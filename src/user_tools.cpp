#include "koroutine/user_tools.h"
namespace koroutine {
SleepAwaiter sleep_for(long long duration_ms) {
  return SleepAwaiter(duration_ms);
}

}  // namespace koroutine