#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// The core0 -> core1 audio command vocabulary.
//
// This is the ONLY thing the two cores say to each other, and it crosses via
// the RP2040's hardware inter-core FIFO as a single 32-bit word. That matters:
// a 32-bit push/pop is atomic in hardware, so there is no shared struct to tear,
// no lock to forget, and no ordering question. Anything wider would need a
// mutex, and a mutex on the audio path is how you get a stall.
//
// Core0 decides WHAT should be said (policy — see interaction.h). Core1 decides
// how it actually sounds (realisation — see phrase.h). Neither reaches into the
// other's state.
// ---------------------------------------------------------------------------

namespace cue {

// R2's emotional register, near enough. Each maps to a family of utterances
// rather than a fixed sound — the specific phrase is improvised on core1, so
// the same cue twice never renders identically.
enum class Kind : uint8_t {
  None = 0,
  Acknowledge,  // short rising chirp — "got it"
  Question,     // rising terminal — "what?"
  Chatter,      // busy, mid-range, several syllables — thinking aloud
  Alarm,        // fast, high, agitated, repetitive — "!!"
  Grumble,      // low, slow, falling — put upon
  Whistle,      // long, wide, expressive — the wistful one
  Stop,         // cut whatever is playing
};

// Layout: kind in the low byte, agitation in the second. The top half is
// deliberately left empty — future fields go there without breaking the
// contract, because unknown bits are ignored on the receiving side.
constexpr uint32_t pack(Kind k, uint8_t agitation) {
  return (uint32_t)k | ((uint32_t)agitation << 8);
}

constexpr Kind kind_of(uint32_t word) {
  return (Kind)(word & 0xFF);
}

constexpr uint8_t agitation_of(uint32_t word) {
  return (uint8_t)((word >> 8) & 0xFF);
}

}  // namespace cue
