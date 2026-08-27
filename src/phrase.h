#pragma once

#include <Arduino.h>

#include "cue.h"

// ---------------------------------------------------------------------------
// Composition: turning a cue into a specific thing to say.
//
// Separate from voice.h on purpose. This file decides WHAT the utterance is —
// how many syllables, what pitches, where it sweeps. voice.cpp decides how to
// turn that into samples. Changing R2's speech patterns should never require
// touching the oscillator, and retuning the oscillator should never change what
// he says.
//
// Core1 only. Uses rng, which keeps per-core state for exactly this reason.
// ---------------------------------------------------------------------------

namespace phrase {

// Long enough for the busiest alarm without being worth heap-allocating.
constexpr uint8_t MAX_SYLLABLES = 8;

// One utterance fragment: a single continuous pitch gesture plus the silence
// that follows it.
struct Syllable {
  float f_start;
  float f_peak;  // the overshoot target — passed through, not stopped at
  float f_end;

  uint16_t dur_ms;
  uint16_t gap_ms;  // silence AFTER this syllable

  uint8_t peak_pct;   // where along the syllable f_peak lands, 0-100
  uint8_t level_pct;  // peak amplitude, 0-100

  uint8_t vibrato_cents;  // 0 disables
  uint8_t vibrato_hz;

  uint8_t harmonic_pct;  // second-harmonic content, 0-100
};

struct Phrase {
  Syllable syllable[MAX_SYLLABLES];
  uint8_t count;
};

// Improvises an utterance. The same kind and agitation never produce the same
// phrase twice — that variability is most of what sells it as speech.
void compose(Phrase& out, cue::Kind kind, uint8_t agitation);

}  // namespace phrase
