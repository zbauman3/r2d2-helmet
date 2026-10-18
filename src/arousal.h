#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// How wound up R2 is. A core system metric, not a sound-engine detail.
//
// Every subsystem that has a rhythm reads this: the grid's churn, the PSI flip
// rate, the periscope's dwell and slew, and the tempo of speech. One number
// drives all of them, so a flurry of trigger presses accelerates the entire
// costume at once and everything eases back down together as it decays.
//
// This used to live inside interaction.cpp, which was wrong — the display has
// no business depending on the module that picks which sound to play.
//
// Core0 only. Reads are PURE: level() is called several times per pass through
// loop() by different modules, so it must be a function of elapsed time and not
// something that decays by being observed.
// ---------------------------------------------------------------------------

namespace arousal {

void begin();

// Called when something happens that should wind him up — currently a trigger
// press. Presses close together escalate; one after a long pause starts fresh.
//
// The level COLLAPSES to zero AROUSAL_HOLD_MS after the last bump rather than
// gliding down. Anything that wants to reflect a flurry after the fact must
// latch the level it saw — see interaction.cpp's pending_level — because by the
// time it asks again, the answer will be zero.
void bump(uint32_t now_ms);

// Holds the level at maximum for a window, overriding both the decay and the
// collapse. For events that are not a button press building up gradually — the
// scream — where the whole costume should go frantic at once.
void pin(uint32_t now_ms, uint16_t hold_ms);

// 0 = calm, 255 = maximally wound up.
uint8_t level(uint32_t now_ms);

// Interpolates between a subsystem's own calm and agitated values by the
// current level. Works in either direction, so it suits both durations (which
// shrink as arousal rises) and amounts (which grow).
//
// Every caller supplies its OWN endpoints on purpose. Sharing the metric is the
// point; sharing the actual rate is not — matching every subsystem's timing
// exactly reads as one clock driving everything, which is the opposite of alive.
uint16_t pace(uint16_t calm, uint16_t agitated, uint32_t now_ms);

// pace() with the response curve pushed late: the result stays near `calm`
// through the low bands and only accelerates as the level approaches the top.
//
// This exists because a linear read of a metric that STEPS is not the same as a
// linear ramp. One trigger press is already AGITATION_STEP — nearly a quarter of
// the range — so a linear subsystem is visibly fast after a single tap and has
// little left to give for the next four. Squaring the input spends most of the
// range on the presses that actually mean something.
//
// Use it where the first press should barely register. Subsystems whose calm and
// agitated endpoints are close enough that the whole span reads as one gesture
// should stay on plain pace().
uint16_t pace_late(uint16_t calm, uint16_t agitated, uint32_t now_ms);

}  // namespace arousal
