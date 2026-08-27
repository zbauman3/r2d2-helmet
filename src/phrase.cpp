#include "phrase.h"

#include <math.h>

#include "config.h"
#include "rng.h"

// The three rules this file exists to implement, from the sound research:
//
//   1. Sweeps are exponential, never linear. Handled in voice.cpp, but the
//      pitches chosen here are picked in log space to match.
//   2. Two or three anchor pitches per utterance, swept between — not a
//      continuous random walk. This is what gives a phrase a shape.
//   3. Overshoot before settling. Arriving at a pitch by passing it and falling
//      back is the single most characteristic R2 gesture; without it the whole
//      thing sounds like a test tone.
//
// Everything else here is in service of those.

namespace phrase {
namespace {

constexpr uint8_t MAX_ANCHORS = 3;

struct Recipe {
  float f_lo, f_hi;        // pitch range for the anchors
  uint8_t syl_min, syl_max;
  uint16_t dur_min, dur_max;
  uint16_t gap_min, gap_max;
  uint8_t overshoot_pct;   // how far past f_end the sweep travels
  uint8_t vibrato_cents;
  uint8_t vibrato_hz;
  uint8_t harmonic_pct;
  uint8_t repeat_pct;      // chance a syllable echoes the previous one
  uint8_t dwell_pct;       // chance a syllable holds its anchor instead of moving
  int8_t terminal;         // final syllable: +1 lifts, -1 drops, 0 leaves alone
  bool arc;                // rise fast, then fall a long way — the scream shape
};

// Hand-tuned per cue. These are the personality — the numbers, not the code.
// Durations here are roughly double the first pass. Checked against the film
// sound effects: R2's syllables sit around 150-400 ms with real gaps between
// them, and the first version's 90-200 ms with 40 ms gaps read as a modem
// rather than a droid. Fewer syllables per utterance, too — the busiest
// recipes were saying too much per press.
//
//    f_lo  f_hi  syl    duration     gap      over vibC vibHz harm rept dwell term  arc
constexpr Recipe kRecipes[] = {
    // None — never used, keeps the enum and the table index-aligned.
    {700, 1300, 1, 1, 200,  260,  0,   0,   12, 0,  0,  10, 0,  0,   0, false},

    // Acknowledge: quick, bright, up. Still the shortest thing he says, but it
    // now has room to be a gesture rather than a blip.
    {620, 1500, 1, 2, 150,  280,  70,  140, 16, 0,  0,  12, 0,  20, +1, false},

    // Question: unhurried, and it lifts at the end. The terminal rise is the
    // whole point, so `rising` matters more here than anywhere else.
    {380, 1250, 2, 3, 220,  400,  100, 200, 18, 15, 5,  15, 10, 25, +1, false},

    // Chatter: busy and directionless, the sound of working through something.
    // Dwell is high — thinking aloud involves a lot of holding one note.
    {300, 1350, 2, 5, 170,  340,  90,  180, 14, 10, 6,  18, 30, 35,  0, false},

    // Alarm: clipped, repetitive, wobbling. Note it is NOT the highest recipe —
    // panic is carried by tempo, stutter and wobble, and pushing pitch up here
    // was what made it hurt. Repeat is heavy on purpose: varied panic reads as
    // merely excited.
    {780, 2000, 3, 6, 100,  200,  50,  100, 24, 55, 12, 25, 55, 30, +1, false},

    // Grumble: low and slow with a falling contour and enough harmonic content
    // to sound like effort rather than a whistle. The low end is where this
    // speaker is most comfortable, so it can afford to take its time.
    {150, 520,  1, 3, 320,  620,  120, 280, 10, 30, 4,  55, 20, 40, -1, false},

    // Whistle: long, wide, expressive. The overshoot is what makes it swoop
    // instead of glide.
    {340, 1900, 1, 2, 560,  1100, 150, 300, 32, 20, 5,  12, 0,  15,  0, false},

    // Affirm — "yes". Bright, clean, two syllables that climb. No vibrato and
    // almost no harmonic content: uncertainty is what wobble reads as, and this
    // has to sound sure of itself.
    {620, 1500, 2, 2, 170,  260,  70,  120, 20, 0,  0,  8,  0,  10, +1, false},

    // Decline — "no". Low, slower, and it falls away at the end. The harmonic
    // content is what stops it sounding merely sad and makes it sound flat and
    // unimpressed instead.
    {200, 620,  2, 2, 240,  380,  90,  160, 14, 18, 4,  48, 15, 25, -1, false},

    // Scream. SUPERSEDED — cue::Kind::Scream now plays the recording in
    // scream_pcm.cpp instead of being synthesised. This entry stays because the
    // table has to remain index-aligned with cue::Kind, and because it is a
    // working fallback if the sample is ever dropped.
    //
    // It is also the reason the sample exists. The one sound in R2's vocabulary
    // that is NOT an ARP patch: it is Ben Burtt screaming under a desk in
    // Lucas's basement, sped up. Voice-derived rather than synth-derived, and
    // the differences are what two oscillators could not cover —
    //
    //   harm 60  a voice is harmonically rich, where everything else here is
    //            dominated by its fundamental. This is the single biggest thing
    //            separating a scream from a whistle.
    //   vibC/Hz  vocal-fold roughness, and sped up: natural vibrato is ~5 Hz,
    //            so tape-sped it lands somewhere near 13.
    //   arc      a scream is not a sweep between notes. It leaps up on the
    //            glottal onset and then falls a long way, which no anchor-hop
    //            contour can produce.
    {240, 2200, 1, 2, 700,  1200, 60,  120, 20, 85, 13, 60, 0,  0,  -1, true},
};

static_assert(sizeof(kRecipes) / sizeof(kRecipes[0]) == (size_t)cue::Kind::Stop,
              "recipe table must cover every cue kind up to Stop");

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Soft ceiling on pitch, applied in log space.
//
// A hard constrain() would pile every excited gesture onto the same maximum, so
// an alarm at full agitation becomes a pinned tone with no contour left — loud,
// piercing, and less expressive than the calm version. This compresses instead:
// untouched below the knee, progressively squeezed above it, asymptotic to
// F_MAX so it can never actually reach the painful range.
//
// Contour survives. A syllable that "wants" to sweep an octave above the knee
// still sweeps, just less far — the shape is preserved, the pain is not.
float soften(float f) {
  if (f <= config::VOICE_F_KNEE) {
    return max(f, config::VOICE_F_MIN);
  }

  const float span = log2f(config::VOICE_F_MAX / config::VOICE_F_KNEE);
  const float excess = log2f(f / config::VOICE_F_KNEE);
  const float squeezed = (span * excess) / (span + excess);

  return config::VOICE_F_KNEE * exp2f(squeezed);
}

// Pitches are chosen in log space so that "somewhere between 300 and 3000 Hz"
// means musically central, not 1650 Hz — which is nearly the top of that range
// to the ear.
float random_pitch(float lo, float hi) {
  const float l = lerp(log2f(lo), log2f(hi), rng::below(1000) / 1000.0f);
  return exp2f(l);
}

uint16_t random_range(uint16_t lo, uint16_t hi) {
  return (hi > lo) ? (lo + rng::below(hi - lo)) : lo;
}

// Agitation compresses time hard. This is now the PRIMARY carrier of
// excitement: speeding a phrase up and clipping its gaps reads as stress just
// as strongly as raising it does, and unlike pitch it does not become painful
// as it goes further.
uint16_t tighten(uint16_t ms, uint8_t agitation) {
  // Eased off from 60% to 50% compression now that the base durations are
  // roughly double. Full agitation should sound hurried, not chattering.
  const uint32_t scaled = (uint32_t)ms * (255 - agitation / 2) / 255;
  return (uint16_t)max<uint32_t>(scaled, 55);
}

// Scales a recipe's wobble with agitation, but only where the recipe already
// wobbles — a cue with no vibrato at rest should not develop a tremor, it
// should just get faster.
uint8_t shake(uint8_t base, uint8_t agitation) {
  if (base == 0) {
    return 0;
  }
  const uint16_t raised = base + (uint16_t)agitation / 5;
  return (uint8_t)min<uint16_t>(raised, 255);
}

}  // namespace

void compose(Phrase& out, cue::Kind kind, uint8_t agitation) {
  const uint8_t index = (uint8_t)kind;
  const Recipe& r = kRecipes[index < (uint8_t)cue::Kind::Stop ? index : 1];

  // Rule 2: a small set of anchors, fixed for the whole utterance. Every
  // syllable travels between two of them, which is what makes a phrase hang
  // together instead of wandering.
  const uint8_t anchors = 2 + rng::below(MAX_ANCHORS - 1);
  float anchor[MAX_ANCHORS];
  for (uint8_t i = 0; i < anchors; ++i) {
    float f = random_pitch(r.f_lo, r.f_hi);

    // A small lift only. Most of the excitement now lives in tempo and stutter.
    f *= 1.0f + (agitation / 255.0f) * (config::AGITATION_PITCH_PCT / 100.0f);
    anchor[i] = soften(f);
  }

  out.count = random_range(r.syl_min, r.syl_max + 1);
  if (out.count > MAX_SYLLABLES) {
    out.count = MAX_SYLLABLES;
  }

  uint8_t from = rng::below(anchors);

  for (uint8_t i = 0; i < out.count; ++i) {
    Syllable& s = out.syllable[i];

    // A stutter: reuse the previous gesture, slightly clipped. R2 doubles back
    // on himself constantly, and a phrase with no repetition at all reads as
    // random tones rather than speech.
    // Stutter gets worse the more wound up he is — repetition is what panic
    // actually sounds like, and it costs nothing in pitch.
    const uint8_t repeat_pct =
        (uint8_t)min<uint16_t>(r.repeat_pct + (uint16_t)agitation / 8, 90);

    if (i > 0 && rng::chance(repeat_pct)) {
      s = out.syllable[i - 1];
      s.dur_ms = (uint16_t)max(35, (int)s.dur_ms * 3 / 4);
      s.gap_ms = random_range(r.gap_min, r.gap_max);
      continue;
    }

    // Sometimes hold the current anchor instead of travelling to another. The
    // first version forced every syllable to move, which made phrases restless
    // — constant back-and-forth with nothing ever landing. A held syllable
    // still has its overshoot, so it bends and settles rather than sitting
    // flat, and that contrast is what gives the moving ones somewhere to go.
    uint8_t to;
    if (rng::chance(r.dwell_pct)) {
      to = from;
    } else {
      to = rng::below(anchors);
      if (to == from && anchors > 1) {
        to = (to + 1) % anchors;
      }
    }

    s.f_start = anchor[from];
    s.f_end = anchor[to];

    // The last syllable carries the utterance's punctuation.
    const bool last = (i + 1 == out.count);
    if (last && r.terminal != 0 && !r.arc) {
      // The interval is small on purpose: it is the DIRECTION that carries the
      // meaning, not the distance. An earlier version lifted by up to 1.65x and
      // that was most of the reason high notes hurt.
      const float interval = 1.12f + rng::below(22) / 100.0f;
      s.f_end = soften((r.terminal > 0) ? s.f_end * interval
                                        : s.f_end / interval);
    }

    if (r.arc) {
      // A scream, not a glide. It leaps to its peak in the first fifth of the
      // syllable and spends the remaining four fifths falling — the arc of a
      // voice, where overshoot-and-settle is the arc of a servo.
      s.f_start = soften(anchor[from] * 1.6f);
      s.f_peak = soften(config::VOICE_F_MAX);
      s.f_end = soften(config::VOICE_F_MIN * 1.7f);
      s.peak_pct = 16 + rng::below(12);
    } else {
      // Rule 3: overshoot. Travel past f_end in whichever direction the sweep is
      // already going, then fall back onto it over the tail of the syllable.
      const float over = 1.0f + r.overshoot_pct / 100.0f;
      s.f_peak = (s.f_end >= s.f_start) ? s.f_end * over : s.f_end / over;
      s.f_peak = soften(s.f_peak);
      s.peak_pct = 62 + rng::below(22);
    }

    s.dur_ms = tighten(random_range(r.dur_min, r.dur_max), agitation);
    s.gap_ms = tighten(random_range(r.gap_min, r.gap_max), agitation);

    // A hair of level variation between syllables. Perfectly even amplitude is
    // one of the things that makes synthesised speech sound mechanical.
    s.level_pct = 78 + rng::below(22);

    s.vibrato_cents = shake(r.vibrato_cents, agitation);
    s.vibrato_hz = shake(r.vibrato_hz, agitation);
    s.harmonic_pct = r.harmonic_pct;

    from = to;
  }

  // Nothing follows the last syllable, so a trailing gap would only delay the
  // next thing R2 wants to say.
  out.syllable[out.count - 1].gap_ms = 0;
}

}  // namespace phrase
