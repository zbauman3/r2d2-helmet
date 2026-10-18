#include <Arduino.h>

#include "arousal.h"
#include "audio.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "interaction.h"
#include "lights.h"
#include "mode.h"
#include "mood.h"
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
//   core0   mode, mood, arousal, buttons, display, lights, periscope,
//           interaction, rng::state[0], Serial
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

  Serial.print(F("trigger  arousal="));
  Serial.println(arousal::level(now_ms));

  display::flourish(now_ms);
  lights::flourish(now_ms);
  periscope::flourish(now_ms);
}

// A deliberate yes or no. Everything that can express it does, together: the
// voice, both PSIs, the screen's palette and the periscope. mood carries the
// window so all four agree on when the moment starts and ends.
void answer(bool yes, uint32_t now_ms) {
  mood::set(yes ? mood::Mood::Positive : mood::Mood::Negative, now_ms);
  interaction::answer(yes, now_ms);
  periscope::answer(yes, now_ms);

  Serial.println(yes ? F("answer: YES") : F("answer: NO"));
}

// The panic scream. Pins arousal at maximum for the duration, so the screen,
// the PSIs and the periscope all go frantic alongside the sound rather than
// carrying on at whatever pace they were at.
void fire_scream(uint32_t now_ms) {
  arousal::pin(now_ms, config::SCREAM_PIN_MS);
  interaction::scream(now_ms);

  display::flourish(now_ms);
  lights::flourish(now_ms);
  periscope::flourish(now_ms);

  Serial.println(F("SCREAM"));
}

// Standby is fully inert: nothing but the mode hold does anything. Sound used to
// fire in either mode, on the reasoning that you might want to bleep at someone
// without lighting up first — in practice that made standby feel like a mode
// that was still half awake, which defeats the point of having one.
bool awake() {
  return mode::get() == mode::Mode::Active;
}

// Bench-only controls, kept even though the buttons work: once the costume is
// assembled they are buried in the shell, and serial is the only way to drive
// it from a laptop.
void poll_serial_control() {
  if (!Serial.available()) {
    return;
  }

  const int c = Serial.read();
  if (c == 'm' || c == 'M') {
    log_mode(mode::toggle());
    return;
  }

  // Gated exactly like the buttons, so the bench path exercises the real
  // behaviour rather than a more permissive version of it.
  if (!awake()) {
    Serial.println(F("(standby — press 'm' first)"));
    return;
  }

  if (c == 't' || c == 'T') {
    fire_interaction(millis());
  } else if (c == 'y' || c == 'Y') {
    answer(true, millis());
  } else if (c == 'n' || c == 'N') {
    answer(false, millis());
  } else if (c == 's' || c == 'S') {
    fire_scream(millis());
  }
}

}  // namespace

void setup() {
  Serial.begin(config::SERIAL_BAUD);
  // Deliberately not waiting for a host to attach. This runs untethered on a
  // costume, where a wait-for-serial loop is an unbootable device.

  rng::begin();
  mode::begin();
  mood::begin();
  arousal::begin();
  buttons::begin();
  interaction::begin();
  display::begin();
  lights::begin();
  periscope::begin();

  Serial.println(F("r2d2-helmet: up, standby."));
  Serial.println(F("  D2 hold = mode ('m')   D2 x2 = no ('n')   D2 x3 = yes ('y')"));
  Serial.println(F("  D2 x5   = scream ('s')"));
  Serial.println(F("  D3 tap  = chirp ('t')  D2 x1 and x4 = nothing, on purpose"));
}

void loop() {
  // One millis() read per pass, shared by every module, so that everything in a
  // given frame agrees on what time it is.
  const uint32_t now_ms = millis();

  // Drain core1's replies before anything reads audio::busy(), so a chirp that
  // finished this pass is not still counted as playing.
  audio::poll(now_ms);

  const buttons::Events pressed = buttons::update(now_ms);

  if (pressed.mode_toggle) {
    log_mode(mode::toggle());
  }

  // Everything else is dead in standby. The gate lives here rather than inside
  // each subsystem because this file is the one place that decides what a press
  // means — the modules below it only report and render.
  if (awake()) {
    if (pressed.scream) {
      fire_scream(now_ms);
    } else if (pressed.answer_yes) {
      answer(true, now_ms);
    } else if (pressed.answer_no) {
      answer(false, now_ms);
    }

    if (pressed.trigger_pressed) {
      fire_interaction(now_ms);
    }
  }

  if (config::BENCH_SERIAL_CONTROL) {
    poll_serial_control();
  }

  interaction::update(now_ms);

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
