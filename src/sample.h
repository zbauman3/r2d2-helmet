#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Playback of recorded audio from flash.
//
// The counterpart to voice.h: same job, opposite method. voice generates every
// sample from an oscillator and an envelope; this one reads them out of a const
// array. Both feed audio.cpp, which picks whichever the current cue needs.
//
// Only the scream uses this, and only because the original is Ben Burtt's own
// voice sped up — a two-oscillator model was never going to reproduce that.
// Everything else stays synthesised, which is what keeps it from ever repeating
// itself exactly.
//
// Core1 only. next_sample() is on the audio hot path.
// ---------------------------------------------------------------------------

namespace sample {

// Data must already be at AUDIO_SAMPLE_RATE — there is no resampling here.
void start(const int16_t* pcm, uint32_t count);

void stop();

bool playing();

// Returns silence (0) when idle, so the caller can pump unconditionally.
int16_t next_sample();

}  // namespace sample
