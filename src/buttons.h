#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// The two hidden pushbuttons.
//
// Both are normally-open switches to GND read with INPUT_PULLUP, so a pressed
// button reads LOW. Debouncing and gesture decoding are entirely in software —
// there is no RC network on either one. See README.md for the wiring.
//
// D3 is immediate: one tap, one chirp, no waiting.
//
// D2 is counted. A single press is DEAD — deliberately, so the button can be
// brushed by accident inside a costume without doing anything. Double is "no",
// triple is "yes", FOUR is dead as a second deliberate buffer, five is the
// scream, and holding it toggles standby/active. Having no
// single-press action is what makes counting free here: with nothing to fire
// immediately, waiting to see whether more presses arrive costs no latency.
// The same scheme on D3 would delay every chirp by the width of the window.
//
// This module reports what the human DID. main.cpp decides what it means.
// ---------------------------------------------------------------------------

namespace buttons {

// Edges and gestures resolved since the previous update(). Returned by value
// rather than stored as consume-once flags: a stored flag is cleared by whoever
// reads it first, which silently breaks the moment a second consumer appears.
struct Events {
  bool trigger_pressed = false;  // D3 tap — fire an interaction
  bool mode_toggle = false;      // D2 held
  bool answer_no = false;        // D2 double press
  bool answer_yes = false;       // D2 triple press
  bool scream = false;           // D2 five presses
};

void begin();

// Call once per pass through loop().
Events update(uint32_t now_ms);

}  // namespace buttons
