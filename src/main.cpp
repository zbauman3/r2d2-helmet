#include <Arduino.h>

#include "audio.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "interaction.h"
#include "lights.h"
#include "mode.h"
#include "periscope.h"
#include "rng.h"

// ---------------------------------------------------------------------------
// Top level.
//
// Core 0: display, lights, buttons, mode, and the decision of what to say.
// Core 1: PWMAudio and sound synthesis, exclusively.
//
// The split is not a performance nicety. A full-screen blit is ~36 ms of
// blocking SPI against ~23 ms of audio buffering, so synthesis on core0 would
// underrun on every redraw; and calling audio::begin() from core1 puts
// PWMAudio's DMA IRQ on core1's NVIC, where the noInterrupts() inside
// NeoPixel::show() cannot reach it. See .claude/skills/firmware/SKILL.md.
//
// THE ONLY THING THAT CROSSES BETWEEN THEM is a 32-bit cue word through the
// hardware FIFO. No shared structs, no locks. Everything else is core-private:
//
//   core0   mode, buttons, display, lights, periscope, interaction,
//           rng::state[0], Serial
//   core1   audio, phrase, voice, rng::state[1]
//
// Serial belongs to core0 alone. Printing from both cores interleaves mid-line
// at best and races the driver at worst, so core1 never logs — if it needs to
// report something, it should be through the FIFO.
//
// If you are tempted to have core1 read mode, or core0 peek at what is playing,
// add a cue for it instead. That boundary is the whole reason this is simple.
//
// This file is where button presses turn into behaviour. The modules below it
// report and render; none of them decides what a press means.
// ---------------------------------------------------------------------------

namespace {

void log_mode(mode::Mode m) {
  Serial.println(m == mode::Mode::Active ? F("mode: ACTIVE") : F("mode: STANDBY"));
}

// Button 2. One self-contained interaction: the screen flourish now, a sound
// alongside it once core 1 exists.
void fire_interaction(uint32_t now_ms) {
  // interaction picks the cue and hands it to core1; it also owns the agitation
  // that makes repeated presses escalate. Read the level back afterwards purely
  // so the log shows what happened.
  interaction::trigger(now_ms);

  Serial.print(F("trigger  agitation="));
  Serial.println(interaction::agitation(now_ms));

  display::flourish(now_ms);
  lights::flourish(now_ms);
  periscope::flourish(now_ms);
}

// Bench-only mode control, kept even though button 1 now exists: once the
// costume is assembled the buttons are buried in the shell, and a serial toggle
// is the only way to drive mode from a laptop.
void poll_serial_control() {
  if (!Serial.available()) {
    return;
  }

  const int c = Serial.read();
  if (c == 'm' || c == 'M') {
    log_mode(mode::toggle());
  } else if (c == 't' || c == 'T') {
    fire_interaction(millis());
  }
}

}  // namespace

void setup() {
  Serial.begin(config::SERIAL_BAUD);
  // Deliberately not waiting for a host to attach. This runs untethered on a
  // costume, where a wait-for-serial loop is an unbootable device.

  rng::begin();
  mode::begin();
  buttons::begin();
  interaction::begin();
  display::begin();
  lights::begin();
  periscope::begin();

  Serial.println(F("r2d2-helmet: up, standby."));
  Serial.println(F("  D2 or 'm' = toggle mode, D3 or 't' = trigger"));
}

void loop() {
  // One millis() read per pass, shared by every module, so that everything in a
  // given frame agrees on what time it is.
  const uint32_t now_ms = millis();

  const buttons::Events pressed = buttons::update(now_ms);

  if (pressed.mode_pressed) {
    log_mode(mode::toggle());
  }

  if (pressed.trigger_pressed) {
    fire_interaction(now_ms);
  }

  if (config::BENCH_SERIAL_CONTROL) {
    poll_serial_control();
  }

  display::update(now_ms);
  lights::update(now_ms);
  periscope::update(now_ms);

  // Deliberately no delay(). Modules gate themselves on timing::Interval; a
  // sleep here would add latency to the buttons for no benefit.
}

// ---------------------------------------------------------------------------
// Core 1.
//
// Defining these two functions is what launches the second core — they are weak
// symbols in the core's main.cpp, and multicore_launch_core1() is called only if
// one of them is defined. There is deliberately nothing else here.
// ---------------------------------------------------------------------------

void setup1() {
  // MUST be called from this core, not from setup(). begin() is what installs
  // PWMAudio's DMA interrupt, and it lands on whichever core calls it.
  audio::begin();
}

void loop1() {
  // Spins. Core1 has no other work, and staying tight on the buffer keeps the
  // latency between a button press and the first sample down to a few
  // milliseconds.
  audio::service();
}
