#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "arousal.h"
#include "config.h"
#include "mode.h"
#include "mood.h"
#include "rng.h"
#include "timing.h"

namespace display {
namespace {

// Hardware SPI on the variant's default SPI0 pins, so no setSCK()/setTX() calls
// are needed — pins_arduino.h already maps them to GP18/GP19.
Adafruit_ST7789 tft(config::PIN_TFT_CS, config::PIN_TFT_DC, config::PIN_TFT_RST);

// --- Palette ---------------------------------------------------------------
//
// RGB565. Blue-dominant with sparse warm accents: R2's logic displays are
// mostly cool with occasional flashes, and an even spread across these colours
// reads as a disco rather than a droid.
//
// ORDER IS LOAD-BEARING. These are sorted by approximate perceived brightness,
// because "nearby on the ramp" is how pick_near() decides which transitions are
// cheap. Reordering them without reordering kPalette and kWeights to match will
// quietly wreck both the look and the tear mitigation.

enum Colour : uint8_t {
  OFF = 0,
  BLUE,
  TEAL,
  RED,
  ORANGE,
  AMBER,
  CYAN,
  WHITE,
  COLOUR_COUNT,
};

constexpr uint16_t kPalette[COLOUR_COUNT] = {
    0x0841,  // OFF    — near-black with a blue tint, not pure black. A totally
             //          black cell reads as a dead pixel; a dim one reads as an
             //          unlit element of a panel that is switched on.
    0x001F,  // BLUE   — pure blue is much darker than it looks written down,
             //          which is why it sits this low on the ramp
    0x0410,  // TEAL   — dim blue-green, fills space without drawing the eye
    0xF800,  // RED
    0xFC00,  // ORANGE
    0xFD20,  // AMBER
    0x07FF,  // CYAN
    0xFFFF,  // WHITE
};

// Selection weights, summing to 100. Roughly half the panel sits dark or dim at
// any moment, which is what gives the lit cells something to read against.
constexpr uint8_t kWeights[COLOUR_COUNT] = {
    28,  // OFF
    22,  // BLUE
    16,  // TEAL
     3,  // RED
     4,  // ORANGE
     8,  // AMBER
    18,  // CYAN
     1,  // WHITE
};

// How far along the ramp a "nearby" transition may travel.
constexpr uint8_t kNearbySpan = 2;

// Mood restricts the palette to one end of the ramp rather than recolouring it.
// Because the ramp is sorted by brightness and the warm colours happen to sit
// in the middle, a negative mood lands on RED/ORANGE/AMBER and a positive one
// on TEAL/CYAN/WHITE — with OFF available to both so the grid keeps its texture
// instead of turning into a solid block.
constexpr uint8_t kNegLo = RED, kNegHi = AMBER;
constexpr uint8_t kPosLo = TEAL, kPosHi = WHITE;

// --- State -----------------------------------------------------------------

constexpr uint8_t kCells = config::GRID_COLS * config::GRID_ROWS;

// Intended colour of each cell, and what is actually on the glass. Diffing the
// two is what keeps this cheap: a full-screen repaint is ~109 KB over SPI, but
// a tick only ever touches the cells that changed.
uint8_t want[kCells];
uint8_t drawn[kCells];

// Sentinel that no palette index can equal, so the first render() after begin()
// paints every cell rather than trusting a zeroed array.
constexpr uint8_t kNeverDrawn = 0xFF;

timing::Interval tick(config::GRID_TICK_CALM_MS);

// Trigger-button reaction. `bursting` is tracked rather than derived from the
// deadline alone because leaving the burst has to restore the tick period, and
// that is an edge, not a condition.
bool bursting = false;
uint32_t burst_until_ms = 0;

// Tracks mode locally rather than asking mode.cpp for a consume-once flag —
// several modules watch mode, and whichever one read the flag first would clear
// it for the others.
mode::Mode last_mode = mode::Mode::Standby;
mood::Mood last_mood = mood::Mood::Neutral;

// --- Helpers ---------------------------------------------------------------

// Weighted pick across an inclusive slice of the ramp. Weighting within the
// window rather than picking uniformly is what stops the constrained walk from
// flattening the palette out to an even spread over time.
uint8_t pick_in_range(uint8_t lo, uint8_t hi) {
  uint16_t total = 0;
  for (uint8_t i = lo; i <= hi; ++i) {
    total += kWeights[i];
  }

  uint32_t roll = rng::below(total);
  for (uint8_t i = lo; i <= hi; ++i) {
    if (roll < kWeights[i]) {
      return i;
    }
    roll -= kWeights[i];
  }
  return lo;  // unreachable
}

uint8_t pick_any() {
  const mood::Mood m = mood::get(millis());
  if (m == mood::Mood::Negative) {
    return rng::chance(30) ? OFF : pick_in_range(kNegLo, kNegHi);
  }
  if (m == mood::Mood::Positive) {
    return rng::chance(30) ? OFF : pick_in_range(kPosLo, kPosHi);
  }
  return pick_in_range(0, COLOUR_COUNT - 1);
}

// The tear mitigation. Most of the time a cell only moves a step or two along
// the brightness ramp, so if the panel catches it mid-write the two halves are
// close enough that the split does not register. The rest of the time it jumps
// anywhere, which is what keeps the panel from looking like it is breathing.
uint8_t pick_near(uint8_t current) {
  // A mood is a statement, so it overrides the nearby-transition smoothing
  // entirely — the grid should change to match the moment, not drift there.
  if (mood::get(millis()) != mood::Mood::Neutral) {
    return pick_any();
  }

  if (!rng::chance(config::GRID_NEARBY_PCT)) {
    return pick_any();
  }

  const uint8_t lo = (current > kNearbySpan) ? (current - kNearbySpan) : 0;
  const uint8_t hi = min<uint8_t>(current + kNearbySpan, COLOUR_COUNT - 1);
  return pick_in_range(lo, hi);
}

// Must be called inside a startWrite()/endWrite() pair — this is the batched
// half of fillRect(), without the per-call transaction.
void paint_cell(uint8_t index) {
  const uint8_t col = index % config::GRID_COLS;
  const uint8_t row = index / config::GRID_COLS;

  // Inset by half the gutter on each side. The gutter itself is background and
  // is never redrawn, which is why begin() paints it once.
  const int16_t x = col * config::CELL_W + config::CELL_GAP / 2;
  const int16_t y = row * config::CELL_H + config::CELL_GAP / 2;

  tft.writeFillRect(x, y, config::CELL_W - config::CELL_GAP,
                    config::CELL_H - config::CELL_GAP, kPalette[want[index]]);

  drawn[index] = want[index];
}

// Pushes only the cells whose intended colour has drifted from the glass.
//
// The whole pass runs inside ONE SPI transaction. fillRect() would open and
// close a transaction per cell, and with a dozen cells changing per tick that
// overhead stretches the window during which the panel is being scanned out
// while half-written — which is what tearing is. Shorter write window, less of
// it. It is also just faster.
void render() {
  uint8_t first = 0;
  while (first < kCells && want[first] == drawn[first]) {
    ++first;
  }

  // Nothing to do. Worth the scan: opening a transaction to draw zero cells is
  // pure cost, and standby hits this path on every single pass through loop().
  if (first == kCells) {
    return;
  }

  tft.startWrite();
  for (uint8_t i = first; i < kCells; ++i) {
    if (want[i] != drawn[i]) {
      paint_cell(i);
    }
  }
  tft.endWrite();
}

void fill_all(uint8_t colour) {
  for (uint8_t i = 0; i < kCells; ++i) {
    want[i] = colour;
  }
}

// Unconstrained on purpose: scatter is only used for mode entry and the trigger
// flourish, where a wholesale change of state is exactly the intent.
void scatter() {
  for (uint8_t i = 0; i < kCells; ++i) {
    want[i] = pick_any();
  }
}

// Re-rolls a slice of the grid. Cells are chosen at random with replacement, so
// the count is approximate — which is fine, and cheaper than shuffling. Real
// logic displays do not update in tidy sweeps.
void churn(uint8_t percent) {
  const uint8_t n = (kCells * percent) / 100;
  for (uint8_t i = 0; i < n; ++i) {
    const uint8_t cell = rng::below(kCells);
    want[cell] = pick_near(want[cell]);
  }
}

// Drops out of the trigger burst and back to the idle rhythm.
void end_burst() {
  bursting = false;
  // No period to restore — update() refreshes it from arousal on the next tick.
}

void on_mode_change(mode::Mode m, uint32_t now_ms) {
  end_burst();

  if (m == mode::Mode::Active) {
    scatter();
  } else {
    // Standby cannot actually go dark: `Lite` is unconnected and pulled high on
    // the 5394, so the backlight is always on and there is no GPIO to kill it.
    // The dark palette entry is as close to "off" as this wiring allows.
    fill_all(OFF);
  }

  // Full period before the first tick in the new state, so a mode change does
  // not land on top of a churn and look like a stutter.
  tick.reset(now_ms);
  last_mode = m;
}

}  // namespace

void begin() {
  // Native portrait dimensions, then rotate — see config::TFT_NATIVE_W.
  tft.init(config::TFT_NATIVE_W, config::TFT_NATIVE_H);
  tft.setSPISpeed(config::TFT_SPI_HZ);
  tft.setRotation(config::TFT_ROTATION);

  // Paints the gutter between cells. Done once because the gutter never
  // changes; every later write is a cell rectangle sitting inside it.
  tft.fillScreen(kPalette[OFF]);

  for (uint8_t i = 0; i < kCells; ++i) {
    want[i] = OFF;
    drawn[i] = kNeverDrawn;
  }

  last_mode = mode::get();
  if (last_mode == mode::Mode::Active) {
    scatter();
  }

  render();
}

void update(uint32_t now_ms) {
  const mode::Mode m = mode::get();
  if (m != last_mode) {
    on_mode_change(m, now_ms);
    render();
    return;
  }

  // Standby is a static frame — no timer work, no SPI traffic.
  if (m != mode::Mode::Active) {
    return;
  }

  if (bursting && (int32_t)(now_ms - burst_until_ms) >= 0) {
    end_burst();
  }

  // Repaint immediately on a mood edge instead of waiting for the next tick, so
  // an answer lands with the sound rather than up to 170 ms behind it.
  const mood::Mood m_now = mood::get(now_ms);
  if (m_now != last_mood) {
    last_mood = m_now;
    scatter();
    render();
    tick.reset(now_ms);
    return;
  }

  // Both the rate and the amount scale with arousal. Cells-per-second is what
  // the eye actually reads, so rate alone was too subtle to notice.
  uint16_t period = arousal::pace(config::GRID_TICK_CALM_MS,
                                  config::GRID_TICK_AGITATED_MS, now_ms);
  uint8_t rate = (uint8_t)arousal::pace(config::GRID_CHURN_CALM_PCT,
                                        config::GRID_CHURN_HOT_PCT, now_ms);

  if (bursting) {
    // Proportional, so the accent sits above whatever is currently happening
    // instead of jumping to a fixed maximum and hiding the arousal ramp.
    period = max<uint16_t>((uint16_t)(period * config::GRID_BURST_TICK_PCT / 100),
                           config::GRID_BURST_TICK_FLOOR_MS);
    rate = min<uint8_t>((uint8_t)(rate * config::GRID_BURST_CHURN_X),
                        config::GRID_BURST_CHURN_CEIL_PCT);
  }

  // Retune BEFORE testing the deadline, not after firing. due() commits the next
  // deadline using whatever period is current, so setting it afterwards leaves
  // one stale interval in the way — long enough that the grid visibly lags the
  // thing that wound it up. retune() also pulls a pending deadline in when the
  // period shrinks, so speeding up takes effect on this tick rather than the
  // next one.
  tick.retune(period, now_ms);

  if (!tick.due(now_ms)) {
    return;
  }

  churn(rate);
  render();
}

void flourish(uint32_t now_ms) {
  // Standby stays dark on purpose: the trigger button is supposed to work
  // without lighting the costume up first.
  if (mode::get() != mode::Mode::Active) {
    return;
  }

  bursting = true;
  burst_until_ms = now_ms + config::GRID_BURST_MS;
  // Period is derived from arousal in update(), so nothing to set here.

  // Repaint immediately rather than waiting for the next tick, so the reaction
  // lands on the press instead of up to a tick later.
  scatter();
  render();
  tick.reset(now_ms);
}

}  // namespace display
