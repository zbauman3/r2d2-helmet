#include "interaction.h"

#include "audio.h"
#include "config.h"
#include "cue.h"
#include "rng.h"

namespace interaction {
namespace {

// Agitation is stored as "what it was at the last press", and the current value
// is DERIVED from how long ago that was. It is deliberately not a running total
// that gets decremented.
//
// The decrementing version worked only because it was read once per press. Now
// that the display and the lights sample it every pass through loop(), a
// mutating read would subtract the same elapsed time over and over and collapse
// to zero within milliseconds. Reads must be pure.
uint8_t peak_level = 0;
uint32_t last_press_ms = 0;
cue::Kind last_kind = cue::Kind::None;
bool ever_pressed = false;

// Weights per cue, in three bands of agitation. Reading down a column shows how
// the register shifts: calm R2 chatters and muses, wound-up R2 alarms and
// snaps. Grumble survives at every level because being put upon is his baseline.
//
// Columns: Acknowledge, Question, Chatter, Alarm, Grumble, Whistle
constexpr uint8_t kCalm[]     = {14, 20, 30,  2, 16, 18};
constexpr uint8_t kRoused[]   = {22, 18, 26, 14, 12,  8};
constexpr uint8_t kAgitated[] = {24,  8, 14, 42,  8,  4};

constexpr uint8_t kFirstKind = (uint8_t)cue::Kind::Acknowledge;
constexpr uint8_t kKindCount = sizeof(kCalm);

static_assert(kFirstKind + kKindCount == (uint8_t)cue::Kind::Stop,
              "weight tables must cover every selectable cue");

// Pure. Safe to call as often as you like, from anywhere on core0.
uint8_t level_at(uint32_t now_ms) {
  if (!ever_pressed) {
    return 0;
  }

  const uint32_t elapsed = now_ms - last_press_ms;
  const uint32_t lost = (elapsed * config::AGITATION_DECAY_PER_SEC) / 1000;
  return (lost >= peak_level) ? 0 : (uint8_t)(peak_level - lost);
}

const uint8_t* band_for(uint8_t level) {
  if (level < 85) return kCalm;
  if (level < 175) return kRoused;
  return kAgitated;
}

cue::Kind choose(uint8_t level) {
  const uint8_t* weights = band_for(level);

  // Zero out whatever just played. Done on a copy of the weights rather than by
  // re-rolling until it differs, so a heavily-weighted cue in an agitated band
  // cannot spin here.
  uint16_t total = 0;
  uint8_t adjusted[kKindCount];
  for (uint8_t i = 0; i < kKindCount; ++i) {
    const bool is_last = ((uint8_t)last_kind == kFirstKind + i);
    adjusted[i] = is_last ? 0 : weights[i];
    total += adjusted[i];
  }

  if (total == 0) {
    return cue::Kind::Chatter;  // only reachable if a band is single-valued
  }

  uint32_t roll = rng::below(total);
  for (uint8_t i = 0; i < kKindCount; ++i) {
    if (roll < adjusted[i]) {
      return (cue::Kind)(kFirstKind + i);
    }
    roll -= adjusted[i];
  }
  return cue::Kind::Chatter;  // unreachable
}

}  // namespace

void begin() {
  peak_level = 0;
  last_kind = cue::Kind::None;
  ever_pressed = false;
}

uint8_t agitation(uint32_t now_ms) {
  return level_at(now_ms);
}

uint16_t pace(uint16_t calm_ms, uint16_t agitated_ms, uint32_t now_ms) {
  const int32_t span = (int32_t)agitated_ms - (int32_t)calm_ms;
  return (uint16_t)((int32_t)calm_ms + (span * level_at(now_ms)) / 255);
}

void trigger(uint32_t now_ms) {
  uint16_t level = level_at(now_ms);

  // Only presses inside the window escalate. A press after a long pause is a
  // fresh conversation, not a continuation of the last one.
  if (ever_pressed && (now_ms - last_press_ms) < config::ESCALATE_WINDOW_MS) {
    level += config::AGITATION_STEP;
  }

  peak_level = (level > 255) ? 255 : (uint8_t)level;
  last_press_ms = now_ms;
  ever_pressed = true;

  const cue::Kind kind = choose(peak_level);
  audio::request(kind, peak_level);
  last_kind = kind;
}

}  // namespace interaction
