#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// The two hidden pushbuttons.
//
// Both are normally-open switches to GND read with INPUT_PULLUP, so a pressed
// button reads LOW. Debouncing is entirely in software — there is no RC network
// on either one. See README.md for the wiring.
//
// This module reports edges and nothing else. It does not know what a press
// means; main.cpp decides that. Keeping the policy out of here is what lets the
// trigger button's behaviour grow without this file changing.
// ---------------------------------------------------------------------------

namespace buttons {

// Edges seen since the previous update(). Returned by value rather than stored
// as consume-once flags: a stored flag is cleared by whoever reads it first,
// which silently breaks the moment a second consumer appears.
struct Events {
  bool mode_pressed = false;     // button 1, D2 — toggle standby/active
  bool trigger_pressed = false;  // button 2, D3 — fire an interaction
};

void begin();

// Call once per pass through loop(). Reports presses only, not releases.
Events update(uint32_t now_ms);

}  // namespace buttons
