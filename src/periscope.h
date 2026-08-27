#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// The periscope servo.
//
// Owns the servo entirely. Same three-function shape as display and lights,
// because it is the same kind of thing: a mode-driven animation that reacts to
// a trigger and is paced by arousal.
//
// Core0. The Servo library is PIO-backed, so the pulse train is generated in
// hardware and keeps running without CPU attention — the interrupt masking
// inside NeoPixel::show() cannot glitch it.
// ---------------------------------------------------------------------------

namespace periscope {

void begin();

// Call every pass through loop() with the shared millis() value.
void update(uint32_t now_ms);

// The periscope half of a trigger interaction: a decisive move somewhere new.
// Does nothing in standby, matching the display and the lights.
void flourish(uint32_t now_ms);

// A deliberate yes or no. Runs a scripted gesture — a nod, or a decisive drop
// and slow recovery — that overrides normal scanning until it finishes.
void answer(bool yes, uint32_t now_ms);

}  // namespace periscope
