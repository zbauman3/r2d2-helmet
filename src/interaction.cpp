#include "interaction.h"

#include "arousal.h"
#include "audio.h"
#include "config.h"
#include "cue.h"
#include "rng.h"

namespace interaction {
namespace {

cue::Kind last_kind = cue::Kind::None;

// One bit, not a counter. Ten presses during one chirp should produce one
// follow-up, not ten — the flurry is expressed through arousal, which is
// already climbing, rather than through a backlog to work off afterwards.
bool pending = false;

// The HIGHEST arousal seen while the throttle was holding.
//
// This has to be latched rather than re-read at fire time. Arousal collapses to
// zero AROUSAL_HOLD_MS after the last press, and a queued utterance typically
// fires a second or more later — so asking again would reliably answer "calm",
// and a frantic flurry would come out as a mild chirp. The screen would show the
// escalation and the voice would not.
uint8_t pending_level = 0;

// Weights per cue, in three bands of agitation. Reading down a column shows how
// the register shifts: calm R2 chatters and muses, wound-up R2 alarms and
// snaps. Grumble survives at every level because being put upon is his baseline.
//
// Columns: Acknowledge, Question, Chatter, Alarm, Grumble, Whistle
constexpr uint8_t kCalm[]     = {14, 20, 30,  2, 16, 18};
constexpr uint8_t kRoused[]   = {22, 18, 26, 14, 12,  8};
constexpr uint8_t kAgitated[] = {24,  8, 14, 42,  8,  4};

constexpr uint8_t kFirstKind = (uint8_t)cue::FIRST_SELECTABLE;
constexpr uint8_t kKindCount = sizeof(kCalm);

static_assert(kFirstKind + kKindCount == (uint8_t)cue::END_SELECTABLE,
              "weight tables must cover every randomly-selectable cue, and stop "
              "short of the deliberate answers");

const uint8_t* band_for(uint8_t level) {
  if (level < 85) return kCalm;
  if (level < 175) return kRoused;
  return kAgitated;
}

cue::Kind choose(uint8_t level) {
  const uint8_t* weights = band_for(level);

  // Zero out whatever just played. Done on a copy of the weights rather than by
  // re-rolling until it differs, so a heavily-weighted cue in an agitated band
  // cannot spin here.
  uint16_t total = 0;
  uint8_t adjusted[kKindCount];
  for (uint8_t i = 0; i < kKindCount; ++i) {
    const bool is_last = ((uint8_t)last_kind == kFirstKind + i);
    adjusted[i] = is_last ? 0 : weights[i];
    total += adjusted[i];
  }

  if (total == 0) {
    return cue::Kind::Chatter;  // only reachable if a band is single-valued
  }

  uint32_t roll = rng::below(total);
  for (uint8_t i = 0; i < kKindCount; ++i) {
    if (roll < adjusted[i]) {
      return (cue::Kind)(kFirstKind + i);
    }
    roll -= adjusted[i];
  }
  return cue::Kind::Chatter;  // unreachable
}

}  // namespace

void begin() {
  last_kind = cue::Kind::None;
  pending = false;
  pending_level = 0;
}

void answer(bool yes, uint32_t now_ms) {
  // Current agitation colours the delivery — a wound-up "no" comes out sharper
  // and faster than a calm one — but arousal::bump() is deliberately NOT called,
  // so this neither escalates nor resets the exchange.
  // A deliberate answer supersedes anything the throttle was holding: being
  // asked a question outranks a chirp that was still waiting its turn.
  pending = false;
  pending_level = 0;
  audio::request(yes ? cue::Kind::Affirm : cue::Kind::Decline, arousal::level(now_ms));

  // last_kind is deliberately not updated either. The no-repeat rule exists to
  // stop the random rotation stalling; an answer is outside that rotation and
  // should not block whatever neutral cue comes next.
}

void speak(uint8_t level) {
  const cue::Kind kind = choose(level);

  if (audio::request(kind, level)) {
    last_kind = kind;
  }
}

void scream(uint32_t now_ms) {
  pending = false;
  pending_level = 0;

  // Deliberately does NOT check audio::busy(). A scream cuts in — core1's
  // voice::start() replaces whatever is mid-utterance, which is the whole point.
  audio::request(cue::Kind::Scream, 255);
}

void trigger(uint32_t now_ms) {
  // The press always counts toward arousal, whether or not it gets to speak.
  // That is what makes a flurry escalate: the level climbs with every press,
  // and the next utterance to actually fire reflects all of them.
  arousal::bump(now_ms);

  const uint8_t level = arousal::level(now_ms);

  if (audio::busy(now_ms)) {
    pending = true;
    if (level > pending_level) {
      pending_level = level;
    }
    return;
  }

  speak(level);
}

void update(uint32_t now_ms) {
  if (!pending || audio::busy(now_ms)) {
    return;
  }

  pending = false;

  // Whichever is higher: the peak of the flurry that queued this, or the level
  // right now if pressing has continued. Never lower than the moment that
  // earned it.
  const uint8_t level = max(pending_level, arousal::level(now_ms));
  pending_level = 0;

  speak(level);
}

}  // namespace interaction
