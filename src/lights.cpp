#include "lights.h"

#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "interaction.h"
#include "mode.h"
#include "rng.h"
#include "timing.h"

namespace lights {
namespace {

// SK6812 in GRB order at 800 kHz, same protocol as WS2812.
Adafruit_NeoPixel chain(config::PIXEL_COUNT, config::PIN_PIXEL_DATA,
                        NEO_GRB + NEO_KHZ800);

// --- Colours ---------------------------------------------------------------
//
// Running the pixels at 3.3 V instead of 5 V is what lets a 3.3 V GPIO drive
// them without a level shifter, but it leaves the blue and green dies short of
// forward-voltage headroom — they are dimmer than red at the same value. These
// are pre-balanced for that: red is pulled well back so it does not swamp blue,
// and yellow carries extra green so it does not drift orange.
//
// Expect to retune these by eye once they are behind the actual diffuser. They
// are starting points, not measurements.

constexpr uint32_t RED    = 0x8C0000;
constexpr uint32_t BLUE   = 0x0000FF;
constexpr uint32_t GREEN  = 0x00B400;
constexpr uint32_t YELLOW = 0x828200;
constexpr uint32_t DARK   = 0x000000;

// --- State -----------------------------------------------------------------

struct Psi {
  uint32_t colour_a;
  uint32_t colour_b;
  bool showing_b;
  timing::Interval timer;
};

Psi psi[config::PIXEL_COUNT] = {
    // Front: below the logic displays, so it shares the screen's field of view.
    {RED, BLUE, false, timing::Interval(config::PSI_FLIP_CALM_MIN_MS)},
    // Rear.
    {GREEN, YELLOW, false, timing::Interval(config::PSI_FLIP_CALM_MIN_MS)},
};

bool bursting = false;
uint32_t burst_until_ms = 0;

mode::Mode last_mode = mode::Mode::Standby;

// Set when a pixel's colour changes, cleared by push(). show() bit-bangs the
// chain with interrupts masked, so it is worth not calling it on every pass
// through loop() just to rewrite the same two values.
bool dirty = false;

// --- Helpers ---------------------------------------------------------------

// Both ends of the range are paced by arousal, so a wound-up R2's PSIs flicker
// faster AND with less spread — steadier and more urgent, the way a nervous
// pulse is. The randomness within the range is what stops the two pixels from
// locking to each other.
uint16_t flip_delay(uint32_t now_ms) {
  if (bursting) {
    return config::PSI_BURST_FLIP_MS;
  }

  const uint16_t lo = interaction::pace(config::PSI_FLIP_CALM_MIN_MS,
                                        config::PSI_FLIP_HOT_MIN_MS, now_ms);
  const uint16_t hi = interaction::pace(config::PSI_FLIP_CALM_MAX_MS,
                                        config::PSI_FLIP_HOT_MAX_MS, now_ms);
  return lo + rng::below(hi - lo);
}

void set_pixel(uint8_t index, uint32_t colour) {
  chain.setPixelColor(index, colour);
  dirty = true;
}

void push() {
  if (!dirty) {
    return;
  }
  chain.show();
  dirty = false;
}

void show_current(uint8_t index) {
  const Psi& p = psi[index];
  set_pixel(index, p.showing_b ? p.colour_b : p.colour_a);
}

// Restarts a PSI with a fresh randomised delay. Called on state changes so the
// first flip after a transition is not inherited from the previous state.
void rearm(uint8_t index, uint32_t now_ms) {
  psi[index].timer.set_period(flip_delay(now_ms));
  psi[index].timer.reset(now_ms);
}

void end_burst(uint32_t now_ms) {
  if (!bursting) {
    return;
  }

  bursting = false;

  // Re-arm from the idle range, otherwise both pixels would carry the burst's
  // short period until their next flip and keep twitching after the reaction
  // has visibly ended.
  for (uint8_t i = 0; i < config::PIXEL_COUNT; ++i) {
    rearm(i, now_ms);
  }
}

void on_mode_change(mode::Mode m, uint32_t now_ms) {
  end_burst(now_ms);

  for (uint8_t i = 0; i < config::PIXEL_COUNT; ++i) {
    if (m == mode::Mode::Active) {
      // Random starting phase so the two do not begin in lockstep, which is
      // the one arrangement that reads as a single blinking circuit.
      psi[i].showing_b = rng::chance(50);
      show_current(i);
    } else {
      // Unlike the screen, these really can go dark — there is no always-on
      // backlight to work around.
      set_pixel(i, DARK);
    }
    rearm(i, now_ms);
  }

  last_mode = m;
}

}  // namespace

void begin() {
  chain.begin();

  // Explicitly blank the chain. NeoPixels hold whatever was last clocked into
  // them across a warm reset, so without this a re-flash can leave the dome lit
  // from the previous run.
  for (uint8_t i = 0; i < config::PIXEL_COUNT; ++i) {
    set_pixel(i, DARK);
  }
  push();

  last_mode = mode::get();
  if (last_mode == mode::Mode::Active) {
    on_mode_change(last_mode, millis());
    push();
  }
}

void update(uint32_t now_ms) {
  const mode::Mode m = mode::get();
  if (m != last_mode) {
    on_mode_change(m, now_ms);
    push();
    return;
  }

  // Standby is two dark pixels — no timers, no bit-banging.
  if (m != mode::Mode::Active) {
    return;
  }

  if (bursting && (int32_t)(now_ms - burst_until_ms) >= 0) {
    end_burst(now_ms);
  }

  for (uint8_t i = 0; i < config::PIXEL_COUNT; ++i) {
    if (psi[i].timer.due(now_ms)) {
      psi[i].showing_b = !psi[i].showing_b;
      show_current(i);
      rearm(i, now_ms);
    }
  }

  push();
}

void flourish(uint32_t now_ms) {
  // Standby stays dark on purpose — the trigger button is meant to work without
  // lighting the costume up.
  if (mode::get() != mode::Mode::Active) {
    return;
  }

  bursting = true;
  burst_until_ms = now_ms + config::PSI_BURST_MS;

  for (uint8_t i = 0; i < config::PIXEL_COUNT; ++i) {
    psi[i].showing_b = !psi[i].showing_b;
    show_current(i);
    rearm(i, now_ms);
  }

  push();
}

}  // namespace lights
