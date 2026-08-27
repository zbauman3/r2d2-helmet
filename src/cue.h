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

  // Deliberate answers. Never chosen at random — they only fire when asked for
  // by name, so they sit after the randomly-selectable range and the weight
  // tables in interaction.cpp stop short of them.
  Affirm,       // yes — bright and rising
  Decline,      // no — low and falling
  Scream,       // the panic scream — rises fast, falls a long way

  Stop,         // cut whatever is playing
};

// Everything from Acknowledge up to (not including) Affirm is fair game for
// random selection. Anything at or past Affirm must be requested explicitly.
constexpr Kind FIRST_SELECTABLE = Kind::Acknowledge;
constexpr Kind END_SELECTABLE = Kind::Affirm;

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

// --- The return channel, core1 -> core0 ------------------------------------
//
// The inter-core FIFO is bidirectional, so replies use the other direction of
// the same mechanism rather than a shared variable. Keeping everything as
// 32-bit words through the FIFO is what lets the whole contract stay
// lock-free — a shared flag would be safe on its own, but it starts a habit
// that stops being safe the moment someone adds a second field to it.

enum class Reply : uint8_t {
  None = 0,
  Finished,  // the current utterance has been fully generated
};

constexpr uint32_t pack_reply(Reply r) {
  return (uint32_t)r;
}

constexpr Reply reply_of(uint32_t word) {
  return (Reply)(word & 0xFF);
}

}  // namespace cue
