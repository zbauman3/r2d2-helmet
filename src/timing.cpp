#include "timing.h"

namespace timing {

bool Interval::due(uint32_t now_ms) {
  // Signed difference, not `now_ms >= next_`. millis() wraps every ~49 days and
  // a naive comparison stalls for the whole second half of the range when it
  // does. Costumes get left plugged in; this is cheap insurance.
  if ((int32_t)(now_ms - next_) < 0) {
    return false;
  }

  next_ = now_ms + period_;
  return true;
}

}  // namespace timing
