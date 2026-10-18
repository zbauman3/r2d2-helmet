#include "arousal.h"

#include "config.h"

namespace arousal {
namespace {

// Stored as "what it was at the last bump", with the current value DERIVED from
// how long ago that was. Deliberately not a running total that gets decremented.
//
// A decrementing version worked only while a single caller read it once per
// press. Now that the display, the lights and the periscope all sample it every
// pass through loop(), a mutating read would subtract the same elapsed time over
// and over and collapse to zero within milliseconds.
uint8_t peak = 0;
uint32_t last_bump_ms = 0;
bool ever = false;

// Overrides everything below it while it lasts.
uint32_t pinned_until_ms = 0;
bool pinned = false;

}  // namespace

void begin() {
  peak = 0;
  last_bump_ms = 0;
  ever = false;
  pinned = false;
  pinned_until_ms = 0;
}

void pin(uint32_t now_ms, uint16_t hold_ms) {
  pinned = true;
  pinned_until_ms = now_ms + hold_ms;

  // Also seed the ordinary state, so a press landing during the pin escalates
  // from maximum rather than from wherever the level happened to be before.
  peak = 255;
  last_bump_ms = now_ms;
  ever = true;
}

uint8_t level(uint32_t now_ms) {
  if (pinned) {
    if ((int32_t)(now_ms - pinned_until_ms) < 0) {
      return 255;
    }
    // Expired. Fall through to the normal path, which will have collapsed by
    // now — the pin ends the way any other flurry does.
    pinned = false;
  }

  if (!ever) {
    return 0;
  }

  const uint32_t elapsed = now_ms - last_bump_ms;

  // The cliff. Past the hold window arousal is simply gone — no glide, no tail.
  // Winding him up takes sustained pressing and stops the instant you do.
  if (elapsed >= config::AROUSAL_HOLD_MS) {
    return 0;
  }

  const uint32_t lost = (elapsed * config::AGITATION_DECAY_PER_SEC) / 1000;
  return (lost >= peak) ? 0 : (uint8_t)(peak - lost);
}

void bump(uint32_t now_ms) {
  uint16_t next = level(now_ms);

  // Only presses inside the hold window escalate. One arriving after the collapse
  // is a fresh conversation, not a continuation — and level() has already
  // returned zero for it, so there would be nothing to build on anyway.
  if (ever && (now_ms - last_bump_ms) < config::AROUSAL_HOLD_MS) {
    next += config::AGITATION_STEP;
  }

  peak = (next > 255) ? 255 : (uint8_t)next;
  last_bump_ms = now_ms;
  ever = true;
}

uint16_t pace(uint16_t calm, uint16_t agitated, uint32_t now_ms) {
  const int32_t span = (int32_t)agitated - (int32_t)calm;
  return (uint16_t)((int32_t)calm + (span * level(now_ms)) / 255);
}

uint16_t pace_late(uint16_t calm, uint16_t agitated, uint32_t now_ms) {
  // Squared in fixed point: (l/255)^2 * 255, which stays exact at both ends —
  // 0 maps to 0 and 255 maps to 255 — so the endpoints a caller supplies are
  // still the endpoints it gets.
  const uint32_t l = level(now_ms);
  const uint8_t shaped = (uint8_t)((l * l) / 255);

  const int32_t span = (int32_t)agitated - (int32_t)calm;
  return (uint16_t)((int32_t)calm + (span * shaped) / 255);
}

}  // namespace arousal
