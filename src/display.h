#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Front logic display on the 1.9" ST7789.
//
// Owns the TFT entirely. Nothing outside this file touches Adafruit_GFX, the
// SPI bus, or the grid state — the whole module is two functions wide so that
// swapping the render style later cannot ripple outward.
//
// Reads mode::get() to decide whether to animate; it is not told what to do.
// ---------------------------------------------------------------------------

namespace display {

// Brings up the panel and paints the initial frame. Core0 only.
void begin();

// Call every pass through loop() with a single shared millis() value. Cheap
// when nothing is due.
void update(uint32_t now_ms);

// The screen half of a trigger-button interaction: a short burst of faster,
// heavier churn so the panel visibly reacts. Silently does nothing in standby —
// button 2 is meant to work without lighting the costume up.
void flourish(uint32_t now_ms);

}  // namespace display
