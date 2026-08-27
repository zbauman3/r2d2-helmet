#include "rng.h"

#include <hardware/structs/rosc.h>
#include <pico/platform.h>

namespace rng {
namespace {

// One generator per core. Only ever touched by its own core, so no
// synchronisation is needed or wanted on a path this hot.
uint32_t state[2] = {0x2545F491u, 0x9E3779B9u};

// Collects 32 bits from the ring oscillator's random bit.
uint32_t harvest() {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 32; ++i) {
    v = (v << 1) | (rosc_hw->randombit & 1u);
    // ROSC output is correlated when sampled back to back; spacing the reads is
    // what makes the bits worth collecting at all.
    delayMicroseconds(5);
  }
  return v;
}

// xorshift32. Chosen over Arduino's random() because it is a handful of
// instructions with no division — core1 rolls dozens of times per utterance and
// core0 re-rolls a dozen display cells every 90 ms.
uint32_t step(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

}  // namespace

void begin() {
  // Without real entropy every power-on replays an identical sequence — the
  // same flicker, the same utterances. On a costume that is genuinely
  // noticeable: it stops reading as a machine thinking and starts reading as a
  // recording.
  for (uint8_t core = 0; core < 2; ++core) {
    const uint32_t seed = harvest();
    // xorshift is undefined for a zero state, and an all-zero harvest is
    // exactly what a dead ROSC would hand back.
    state[core] = seed ? seed : (0x2545F491u + core);
  }
}

uint32_t next() {
  return step(state[get_core_num()]);
}

uint32_t below(uint32_t n) {
  return n ? next() % n : 0;
}

bool chance(uint8_t percent) {
  return below(100) < percent;
}

}  // namespace rng
