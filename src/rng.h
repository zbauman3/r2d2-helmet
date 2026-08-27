#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Shared pseudo-random source, with INDEPENDENT PER-CORE STATE.
//
// The per-core part is not an optimisation, it is a correctness requirement.
// Core0 rolls for the display and the light rhythms; core1 rolls for every
// pitch and duration in an utterance. A single shared `state` would be a
// read-modify-write race on every call — Cortex-M0+ has no exclusive
// load/store, so the two cores would silently corrupt each other's stream and
// occasionally hand back correlated values.
//
// Each core therefore gets its own generator, seeded separately.
//
// Note that core1 is launched BEFORE setup() runs — the core calls
// multicore_launch_core1() and then setup(), so setup() and setup1() overlap.
// Seeding both slots from core0 is still safe, for two reasons: state[] has
// valid non-zero static initialisers, so an early read gets a working generator
// rather than a corrupt one; and core1 does not roll at all until a cue arrives,
// which cannot happen before core0 has finished setup() and seen a button.
// ---------------------------------------------------------------------------

namespace rng {

// Seeds both cores' streams from the RP2040 ring oscillator. Call once, from
// core0's setup(), BEFORE setup1() can run.
void begin();

uint32_t next();

// Uniform in [0, n). Not perfectly unbiased — modulo skews the low values
// slightly — which is irrelevant for flicker and would be a bug in a lottery.
uint32_t below(uint32_t n);

// True `percent` of the time.
bool chance(uint8_t percent);

}  // namespace rng
