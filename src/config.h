#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware constants. Single source of truth for anything the wiring dictates.
//
// This file MUST agree with .claude/skills/hardware/SKILL.md. If a pin moves on
// the breadboard it moves here, and nowhere else — no module hardcodes a pin.
// ---------------------------------------------------------------------------

namespace config {

// --- Pins (Arduino numbering == GPIO numbering on the KB2040) --------------
// Silkscreen labels are given because that is what is printed on the board and
// what the wiring tables in README.md refer to.

constexpr uint8_t PIN_AUDIO_PWM   = 8;   // D8  — PWM out into the RC filter
constexpr uint8_t PIN_PIXEL_DATA  = 5;   // D5  — NeoPixel data, 470R in series
constexpr uint8_t PIN_BTN_MODE    = 2;   // D2  — button 1, toggles active mode
constexpr uint8_t PIN_BTN_TRIGGER = 3;   // D3  — button 2, fires an interaction
constexpr uint8_t PIN_TFT_DC      = 7;   // D7
constexpr uint8_t PIN_TFT_RST     = 4;   // D4
constexpr uint8_t PIN_TFT_CS      = 10;  // D10
constexpr uint8_t PIN_SERVO       = 6;   // D6  — periscope servo

// SPI0 (SCK=GP18, MOSI=GP19) is the variant default, so the plain `SPI` object
// is already on the right pins. Deliberately not listed here: naming them would
// imply we configure them, and we must not.

// --- Display ---------------------------------------------------------------

// Adafruit 5394: the ST7789 controller is addressed in its native portrait
// orientation, then rotated. Passing the rotated size to init() puts the
// controller's row/column offsets in the wrong place and shifts the image.
constexpr uint16_t TFT_NATIVE_W = 170;
constexpr uint16_t TFT_NATIVE_H = 320;

// 1 and 3 both give landscape and differ by 180 degrees. Which one is "up"
// depends on how the panel ends up mounted in the dome — expect to flip this.
constexpr uint8_t TFT_ROTATION = 1;

// 12 and 24 MHz were tested against the tearing artefact and are perceptually
// identical, which is expected: the panel takes ~1.5 ms to scan across one
// cell's worth of rows, and a cell write is 0.3-0.6 ms either way. The scan is
// the wide window, not the write, so clock speed cannot close it. 24 MHz is
// kept purely for the headroom.
//
// Signal integrity, if it ever shows up, looks different — speckled or
// wrong-coloured cells rather than clean geometric halves. Halve this if so.
constexpr uint32_t TFT_SPI_HZ = 24000000;

constexpr uint16_t SCREEN_W = 320;
constexpr uint16_t SCREEN_H = 170;

// --- Logic display grid ----------------------------------------------------
//
// R2's front logic displays are LED matrices, not smooth graphics, so the panel
// imitates one: a coarse grid of lit blocks. 10x5 divides both screen
// dimensions exactly (320/10 = 32, 170/5 = 34), which avoids rounding leftovers
// at the right and bottom edges that would read as a misaligned panel.

constexpr uint8_t GRID_COLS = 10;
constexpr uint8_t GRID_ROWS = 5;
constexpr uint8_t CELL_W    = SCREEN_W / GRID_COLS;  // 32
constexpr uint8_t CELL_H    = SCREEN_H / GRID_ROWS;  // 34
constexpr uint8_t CELL_GAP  = 4;                     // dark gutter between cells

static_assert(GRID_COLS * CELL_W == SCREEN_W, "grid must tile the screen exactly");
static_assert(GRID_ROWS * CELL_H == SCREEN_H, "grid must tile the screen exactly");

// How often the grid mutates, and how much of it changes each time. Real logic
// displays update fast and irregularly; too slow reads as a screensaver, too
// fast reads as static.
// The grid's rhythm tracks agitation, same as the voice and the lights — see
// interaction::pace(). Calm is deliberately unhurried; a logic display that
// strobes constantly reads as a fault light rather than a machine thinking.
constexpr uint16_t GRID_TICK_CALM_MS     = 170;
constexpr uint16_t GRID_TICK_AGITATED_MS = 70;
constexpr uint8_t  GRID_CHURN_PCT        = 25;  // cells re-rolled per tick

// The 5394 breaks out no TE pin, so writes cannot be synced to the panel's
// refresh and a cell caught mid-scan shows a torn edge for one frame. That is
// unfixable. What IS fixable is how much a tear stands out: a cell tearing
// between two similar colours is invisible, one tearing between black and white
// is not.
//
// So most transitions are restricted to a couple of steps along the palette's
// brightness ramp, and only the remainder jump anywhere. Set to 0 for the
// original behaviour — every change a free-for-all, and every tear obvious.
constexpr uint8_t GRID_NEARBY_PCT = 75;

// The trigger button's screen flourish: a short window of faster, heavier churn
// so the panel visibly reacts rather than carrying on at its idle rhythm.
constexpr uint16_t GRID_BURST_MS        = 700;
constexpr uint16_t GRID_BURST_TICK_MS   = 45;
constexpr uint8_t  GRID_BURST_CHURN_PCT = 70;

// --- Dome lights (PSIs) ----------------------------------------------------
//
// R2's two Process State Indicators: a round lens on the front of the dome
// below the logic displays, and a matching one on the back. Each alternates
// between two colours on its own irregular rhythm. The canonical pairs are
// red/blue at the front and green/yellow at the back, which is why the two
// pixels get different palettes rather than being driven as a pair.

constexpr uint8_t PIXEL_COUNT = 2;

// Chain order. Pixel 0 is whichever one has its DIN wired to GP5 through the
// 470R — so the FRONT pixel is first in the chain and the rear hangs off its
// DOUT. Swap these two constants rather than resoldering if it ends up backwards.
constexpr uint8_t PIXEL_FRONT = 0;
constexpr uint8_t PIXEL_REAR  = 1;

// How long a PSI holds a colour before flipping. Randomised per flip and per
// pixel: a fixed period reads as a blinking LED, and two pixels sharing a
// period read as one circuit driving both. Irregular and independent is what
// makes it look like two things thinking separately.
// Endpoints, interpolated by agitation. The calm range is wide and slow: real
// PSIs sit on a colour for seconds at a time. The agitated range is faster but
// still wider than the screen's, so the two subsystems accelerate together
// without locking into the same rhythm.
constexpr uint16_t PSI_FLIP_CALM_MIN_MS = 1100;
constexpr uint16_t PSI_FLIP_CALM_MAX_MS = 3800;
constexpr uint16_t PSI_FLIP_HOT_MIN_MS  = 320;
constexpr uint16_t PSI_FLIP_HOT_MAX_MS  = 1150;

// Trigger reaction. Matches GRID_BURST_MS so the lights and the screen react
// as one event rather than two overlapping ones.
constexpr uint16_t PSI_BURST_MS      = 700;
constexpr uint16_t PSI_BURST_FLIP_MS = 70;

// --- Periscope servo (TowerPro SG92R) --------------------------------------
//
// A hinge that waves the periscope vertically, driven across roughly 180
// degrees of servo travel.
//
// The library's DEFAULT_MIN/MAX_PULSE_WIDTH are 1000/2000 us, which is about
// HALF this servo's range — the arm would only sweep ~90 degrees and the motion
// would look timid for no obvious reason. 500/2500 us is the SG92R's real span
// and must be passed to attach() explicitly.
constexpr uint16_t SERVO_MIN_US = 500;
constexpr uint16_t SERVO_MAX_US = 2500;

// Mechanical limits, not electrical. Kept inside 0-180 so the horn never drives
// into the end of its travel and stalls against the hinge — a stalled SG92R
// pulls ~650 mA and buzzes until something gives. Trim these to whatever the
// printed parts actually allow.
constexpr uint8_t PERISCOPE_ANGLE_MIN = 15;
constexpr uint8_t PERISCOPE_ANGLE_MAX = 165;
constexpr uint8_t PERISCOPE_PARK      = 20;  // stowed position for standby

// Update period. 20 ms matches the servo's own 50 Hz frame — stepping the
// commanded angle faster than the servo is refreshed just wastes work.
constexpr uint16_t PERISCOPE_STEP_MS = 20;

// Paced by agitation, like everything else — but deliberately the LEAST
// responsive of the three. A head that snaps around as fast as the lights
// flicker reads as a machine fault; it should look attentive, not frantic.
constexpr uint16_t PERISCOPE_DWELL_CALM_MS = 3000;
constexpr uint16_t PERISCOPE_DWELL_HOT_MS  = 1150;
constexpr uint8_t  PERISCOPE_SLEW_CALM_DPS = 65;   // degrees per second
constexpr uint8_t  PERISCOPE_SLEW_HOT_DPS  = 175;

// How the movement is chosen. Most moves are small excursions around a focus
// point; occasionally the focus itself jumps. That contrast is what reads as
// looking at something rather than twitching at random.
constexpr uint8_t PERISCOPE_SCAN_SPREAD  = 26;  // degrees either side of focus
constexpr uint8_t PERISCOPE_REFOCUS_PCT  = 24;  // chance of a large move
constexpr uint8_t PERISCOPE_HOLD_PCT     = 16;  // chance of simply staying put
constexpr uint8_t PERISCOPE_RECHECK_PCT  = 45;  // double-take after a big move

// --- Buttons ---------------------------------------------------------------

// Both buttons are bare switches with no RC network, so every bounce arrives at
// the pin. 25 ms comfortably outlasts tactile switch chatter while staying well
// under the ~100 ms where a press starts to feel unresponsive. The run from the
// breadboard out to a button buried in the costume shell is long and unshielded,
// which argues for the higher end of the usual range rather than the lower.
constexpr uint16_t BUTTON_DEBOUNCE_MS = 25;

// --- Audio -----------------------------------------------------------------

constexpr uint32_t AUDIO_SAMPLE_RATE = 22050;

// 130 kHz keeps clear of the PAM8302A's ~250 kHz switching. 260 kHz (9-bit at
// 133 MHz) would beat against it at 10 kHz, in the middle of the audio band —
// see .claude/skills/r2-sounds/SKILL.md. Do not "optimise" this.
constexpr uint32_t AUDIO_CARRIER_HZ = 130000;

// 4 x 256 words = 1024 samples ~= 46 ms. Core1 has nothing else to do so this
// should never be needed, which is exactly why it is worth having: it is the
// margin for the case that was not anticipated.
constexpr size_t AUDIO_BUFFERS      = 4;
constexpr size_t AUDIO_BUFFER_WORDS = 256;

// Envelopes and pitch are recomputed every N samples rather than every sample —
// the standard control-rate/audio-rate split. 16 gives ~1.4 kHz control rate,
// fast enough that sweeps have no audible stepping, slow enough that the float
// maths costs a couple of percent of a core with no FPU.
constexpr uint8_t VOICE_CONTROL_DIV = 16;

// Fundamental range.
//
// The ceiling is not about the electronics — it is about ears. Human hearing
// peaks in sensitivity between roughly 2 and 5 kHz thanks to ear-canal
// resonance, so a tone up there is both louder and sharper than the same
// amplitude anywhere else. Sitting at 4 kHz is genuinely painful at close range,
// which is not the effect wanted from a costume people lean in towards.
//
// F_KNEE is where compression starts; F_MAX is the asymptote it approaches but
// never reaches. Between them, pitch is squeezed in log space rather than
// clipped — see soften() in phrase.cpp. That matters: a hard clamp flattens the
// contour of an excited phrase into a pinned tone, losing the gesture at exactly
// the moment it should be most expressive.
constexpr float VOICE_F_MIN  = 150.0f;
constexpr float VOICE_F_KNEE = 1700.0f;
constexpr float VOICE_F_MAX  = 3000.0f;

// Rough equal-loudness compensation. The ear's sensitivity rise means a high
// syllable at the same amplitude as a low one reads as much louder, so level is
// tapered off as pitch climbs. Applied at control rate, so it tracks a sweep
// rather than being fixed per syllable.
constexpr float   VOICE_TILT_FROM_HZ = 1400.0f;
constexpr uint8_t VOICE_TILT_PCT     = 40;  // level reduction at the ceiling

// How much agitation lifts pitch. Deliberately small. Excitement is carried by
// tempo, wobble and stutter instead — see phrase.cpp — because raising pitch is
// the one route to "excited" that also hurts.
constexpr uint8_t AGITATION_PITCH_PCT = 12;

// --- Interaction pattern ---------------------------------------------------
//
// Presses inside this window read as one escalating exchange rather than as
// independent events. Agitation drives which cue gets picked, so hammering the
// button makes R2 progressively more strung out instead of just louder.
constexpr uint16_t ESCALATE_WINDOW_MS      = 2400;
constexpr uint8_t  AGITATION_STEP          = 58;
constexpr uint8_t  AGITATION_DECAY_PER_SEC = 40;

// --- Bench aids ------------------------------------------------------------

// The bring-up order wires the display before the buttons, so mode has to be
// drivable without them. Send 'm' over serial to toggle. Harmless to leave on.
constexpr bool BENCH_SERIAL_CONTROL = true;

constexpr uint32_t SERIAL_BAUD = 115200;

}  // namespace config
