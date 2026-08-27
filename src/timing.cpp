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

void Interval::retune(uint32_t period_ms, uint32_t now_ms) {
  period_ = period_ms;

  const uint32_t soonest = now_ms + period_ms;
  if ((int32_t)(next_ - soonest) > 0) {
    next_ = soonest;
  }
}

}  // namespace timing
