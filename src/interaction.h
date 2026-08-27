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
// Arousal itself lives in arousal.h — this module drives it and reads it, but
// does not own it. The display and the lights have no business depending on the
// module that picks which sound to play.
//
// Core0 only. Owns policy; core1 owns realisation.
// ---------------------------------------------------------------------------

namespace interaction {

void begin();

// Call on a trigger press.
//
// THROTTLED, not queued. A press while R2 is already talking does not interrupt
// him and does not stack up: it raises arousal and marks that one more thing
// wants saying. Whatever the count, exactly one follow-up utterance fires when
// the current one finishes, chosen at the arousal level reached BY THEN rather
// than at the moment any particular button went down.
//
// Cutting an utterance short to start the next was the old behaviour and it made
// a flurry of presses sound like stuttering machinery rather than someone
// getting steadily more worked up.
void trigger(uint32_t now_ms);

// The panic scream. Interrupts whatever is playing — it is supposed to cut in —
// and clears any throttled press, which is now irrelevant next to it.
void scream(uint32_t now_ms);

// Releases a throttled press once the voice frees up. Call once per pass
// through loop(), after audio::poll().
void update(uint32_t now_ms);

// A deliberate yes or no. Bypasses cue selection entirely — this is not R2
// choosing what to say, it is being told. Reads agitation so the delivery still
// matches his state, but does NOT escalate it: an answer is communication, not
// frustration, and hammering out three "no"s should not wind him into an alarm.
void answer(bool yes, uint32_t now_ms);

}  // namespace interaction
