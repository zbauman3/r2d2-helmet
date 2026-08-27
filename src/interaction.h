#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// What R2 chooses to say, and why.
//
// This is the pattern logic behind the trigger button — deliberately not a
// uniform random pick from a bag of sounds. Two things shape it:
//
//   Agitation. Presses close together read as one escalating exchange, so
//   hammering the button winds him up rather than replaying the same register.
//   It decays back down on its own.
//
//   Memory. The cue that just fired is excluded from the next choice, because
//   an immediate repeat is the single most obvious tell that nothing is
//   actually deciding anything.
//
// It is also the shared AROUSAL source for the whole of core0. The display's
// churn rate and the PSI flip rate both read agitation through pace(), so a
// flurry of presses speeds up the screen and the lights along with the voice.
// They are given different endpoints on purpose — matching them exactly reads
// as one clock driving everything, which is the opposite of alive.
//
// Core0 only. Owns policy; core1 owns realisation.
// ---------------------------------------------------------------------------

namespace interaction {

void begin();

// Call on a trigger press. Picks a cue and hands it to core1.
void trigger(uint32_t now_ms);

// 0-255, for anything that wants to react to how wound up he is. Pure — safe to
// call every pass through loop().
uint8_t agitation(uint32_t now_ms);

// Interpolates a subsystem's own calm and agitated timings by the current
// arousal. Each caller supplies its own endpoints, so everything accelerates
// together without running at literally the same rate.
uint16_t pace(uint16_t calm_ms, uint16_t agitated_ms, uint32_t now_ms);

}  // namespace interaction
