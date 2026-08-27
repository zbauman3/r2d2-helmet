#pragma once

#include <Arduino.h>

#include "phrase.h"

// ---------------------------------------------------------------------------
// Synthesis: turning a composed phrase into samples.
//
// A single-voice oscillator with a pitch envelope and an amplitude envelope.
// Knows nothing about cues, buttons, or PWMAudio — hand it a Phrase and pull
// samples until it says it is done.
//
// Core1 only. next_sample() is on the audio hot path and is called 22050 times
// a second, so it does integer work only; everything involving floats happens
// at control rate.
// ---------------------------------------------------------------------------

namespace voice {

void begin(uint32_t sample_rate);

// Replaces whatever is playing. Interrupting mid-syllable is intentional — R2
// talking over himself when jabbed twice is more alive than a queue.
void start(const phrase::Phrase& p);

void stop();

bool speaking();

// Returns silence (0) when idle rather than nothing at all, so the caller can
// keep the DMA fed unconditionally and never has to handle an underflow.
int16_t next_sample();

}  // namespace voice
