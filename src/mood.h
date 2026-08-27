#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Short-lived emotional colour, shared across the whole of core0.
//
// A deliberate yes or no is not just a sound — the lights go red or green, the
// periscope nods or slumps, and the screen shifts warm or cool. Those three
// need to agree on when the moment starts and when it is over, so the window
// lives here rather than being timed three separate times and drifting apart.
//
// Distinct from `mode` (standby/active, which is durable and user-driven) and
// from `interaction`'s agitation (which is a continuous level, not an event).
// This is a brief, decaying overlay on top of both.
//
// Core0 only.
// ---------------------------------------------------------------------------

namespace mood {

enum class Mood : uint8_t {
  Neutral,
  Positive,
  Negative,
};

void begin();

// Starts (or restarts) the window.
void set(Mood m, uint32_t now_ms);

// Expires on its own — no one has to clear it. Pure, so every subsystem can
// call it every pass through loop().
Mood get(uint32_t now_ms);

}  // namespace mood
