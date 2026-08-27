#include "mood.h"

#include "config.h"

namespace mood {
namespace {

Mood current = Mood::Neutral;
uint32_t until_ms = 0;

}  // namespace

void begin() {
  current = Mood::Neutral;
  until_ms = 0;
}

void set(Mood m, uint32_t now_ms) {
  current = m;
  until_ms = now_ms + config::MOOD_HOLD_MS;
}

Mood get(uint32_t now_ms) {
  if (current == Mood::Neutral) {
    return Mood::Neutral;
  }

  // Signed compare, for the same millis() rollover reason as timing::Interval.
  if ((int32_t)(now_ms - until_ms) >= 0) {
    return Mood::Neutral;
  }
  return current;
}

}  // namespace mood
