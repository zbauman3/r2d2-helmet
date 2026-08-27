#pragma once

#include <Arduino.h>

#include "cue.h"

// ---------------------------------------------------------------------------
// The core1 audio engine. Owns PWMAudio, the phrase composer and the voice.
//
// WHY CORE1, restated because it is the load-bearing decision of the firmware:
//
//   1. A full-screen blit is ~36 ms of blocking SPI. Default audio buffering is
//      ~23 ms. Sharing a core underruns on every screen redraw.
//   2. Adafruit_NeoPixel::show() masks interrupts for the whole transfer.
//      begin() is called from core1, which installs PWMAudio's DMA IRQ on
//      core1's NVIC — and PRIMASK is per-core on Cortex-M0+, so core0's
//      blackout cannot touch it.
//
// Everything below this header is core1-private. The only channel in is the
// FIFO, and the only thing that crosses it is a 32-bit cue word.
// ---------------------------------------------------------------------------

namespace audio {

// Call from setup1(). Must not be called from core0 — doing so would put the
// DMA interrupt on the wrong core and quietly undo reason 2 above.
void begin();

// Call from loop1(). Drains pending cues and keeps the DMA buffer fed.
void service();

// Core0 side. Non-blocking: if the FIFO is full the cue is dropped rather than
// stalling the display loop, which is the right trade — a missed bleep beats a
// frozen costume.
bool request(cue::Kind kind, uint8_t agitation);

}  // namespace audio
