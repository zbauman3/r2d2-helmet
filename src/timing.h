#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Non-blocking periodic timing.
//
// Nothing in this project may call delay() outside of setup: core0 drives the
// display, the lights and both buttons from a single loop, so a blocked loop is
// a frozen costume. Interval is the only sanctioned way to do "every N ms".
// ---------------------------------------------------------------------------

namespace timing {

class Interval {
 public:
  explicit Interval(uint32_t period_ms) : period_(period_ms), next_(0) {}

  // True once per period. Pass the same millis() value to every due() call in a
  // given pass through loop() so that all modules agree on "now".
  bool due(uint32_t now_ms);

  void set_period(uint32_t period_ms) { period_ = period_ms; }

  // Restart the period from now — use after a state change so the first tick in
  // the new state is a full period away rather than firing immediately.
  void reset(uint32_t now_ms) { next_ = now_ms + period_; }

  // Change the period AND pull in the pending deadline if the new period is
  // shorter. set_period() alone is not enough for a rate that varies
  // continuously: the deadline was already committed under the old period, so
  // speeding up would still have to wait out one full slow interval first —
  // which is exactly long enough to make the change feel disconnected from
  // whatever caused it. Never extends a pending deadline, so slowing down
  // cannot cause a double tick.
  void retune(uint32_t period_ms, uint32_t now_ms);

 private:
  uint32_t period_;
  uint32_t next_;
};

}  // namespace timing
