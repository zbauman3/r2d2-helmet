#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// The two dome Process State Indicators.
//
// Front PSI alternates red/blue, rear alternates green/yellow — the canonical
// pairs. Each runs on its own irregular rhythm, so they drift in and out of
// phase rather than blinking together.
//
// Owns the NeoPixel chain entirely. Deliberately mirrors display's interface:
// both are mode-driven animations with a trigger reaction, and keeping the
// shape identical means main.cpp treats them the same way.
// ---------------------------------------------------------------------------

namespace lights {

void begin();

// Call every pass through loop() with the shared millis() value. Only touches
// the chain when something actually changed.
void update(uint32_t now_ms);

// The lights half of a trigger interaction: both PSIs flip rapidly for a short
// window. Does nothing in standby, matching the display.
void flourish(uint32_t now_ms);

}  // namespace lights
