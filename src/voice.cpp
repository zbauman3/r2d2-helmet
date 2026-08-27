#include "voice.h"

#include <math.h>

#include "config.h"

namespace voice {
namespace {

// --- Oscillator ------------------------------------------------------------

constexpr uint16_t kTableSize = 256;
constexpr uint32_t kTableShift = 24;  // top 8 bits of the phase index the table
int16_t sine_table[kTableSize];

// Arduino's PI is a double literal, and multiplying a float by it silently
// promotes the whole expression to soft double — which on a core with no FPU is
// several times the cost of soft float, for no benefit.
constexpr float kTwoPi = 6.28318531f;

// A pure sine is the right primitive here. Burtt's original was an oscillator
// through a self-resonating filter, which is dominated by its fundamental; and
// anything with real harmonic content would alias at 22 kHz once the pitch
// sweeps up past a couple of kHz.
int32_t osc(uint32_t phase) {
  const uint16_t i = phase >> kTableShift;
  const uint16_t frac = (phase >> 16) & 0xFF;

  const int32_t a = sine_table[i];
  const int32_t b = sine_table[(i + 1) & (kTableSize - 1)];

  // Interpolated, because a raw 256-entry lookup quantises to about -48 dB of
  // noise, which is audible on a held tone.
  return a + (((b - a) * frac) >> 8);
}

// --- State -----------------------------------------------------------------

uint32_t rate = config::AUDIO_SAMPLE_RATE;
float hz_to_phase = 0.0f;  // multiply a frequency by this to get a phase step

phrase::Phrase script;
uint8_t index = 0;
bool playing = false;
bool in_gap = false;

uint32_t phase = 0;
uint32_t phase_step = 0;

uint32_t pos = 0;      // samples into the current syllable or gap
uint32_t seg_len = 0;  // its length in samples

int32_t level = 0;  // amplitude, Q15
uint32_t attack_len = 0;
uint32_t release_len = 0;

// Pitch endpoints cached in log2 space. Interpolating linearly here is what
// makes the sweep exponential in frequency — rule 1 — and it costs one exp2f
// per control tick instead of a powf per sample.
float log_start = 0.0f;
float log_peak = 0.0f;
float log_end = 0.0f;
float peak_at = 0.75f;

// Equal-loudness taper, precomputed in log space so the control tick is two
// compares and a multiply.
float log_tilt_from = 0.0f;
float log_tilt_span = 1.0f;

float vib_phase = 0.0f;
uint8_t control_countdown = 0;
int32_t harmonic = 0;  // 0-256

uint32_t ms_to_samples(uint16_t ms) {
  return ((uint32_t)ms * rate) / 1000;
}

void begin_syllable() {
  const phrase::Syllable& s = script.syllable[index];

  log_start = log2f(s.f_start);
  log_peak = log2f(s.f_peak);
  log_end = log2f(s.f_end);
  peak_at = s.peak_pct / 100.0f;

  seg_len = max<uint32_t>(ms_to_samples(s.dur_ms), 2);
  pos = 0;

  // Envelope edges exist to stop clicks as much as to shape the sound: any
  // discontinuity at a syllable boundary is a step change into a class-D amp,
  // and it will be heard. Both are clamped so short syllables still get one.
  attack_len = min<uint32_t>(ms_to_samples(6), seg_len / 3);
  release_len = min<uint32_t>(ms_to_samples(24), seg_len / 3);
  if (attack_len == 0) attack_len = 1;
  if (release_len == 0) release_len = 1;

  harmonic = ((int32_t)s.harmonic_pct * 256) / 100;
  vib_phase = 0.0f;
  control_countdown = 0;
  in_gap = false;
}

void begin_gap() {
  const uint16_t ms = script.syllable[index].gap_ms;
  if (ms == 0) {
    // Straight into the next syllable rather than burning a control tick on a
    // zero-length gap.
    pos = 0;
    seg_len = 0;
    in_gap = true;
    return;
  }

  seg_len = ms_to_samples(ms);
  pos = 0;
  in_gap = true;
}

void advance() {
  ++index;
  if (index >= script.count) {
    playing = false;
    level = 0;
    return;
  }
  begin_syllable();
}

// Runs every VOICE_CONTROL_DIV samples. All the float maths lives here.
void control_tick() {
  const phrase::Syllable& s = script.syllable[index];
  const float t = (float)pos / (float)seg_len;

  // Two log-space segments: out to the overshoot peak, then back onto the
  // target. The corner between them is what you hear as the "settle".
  float log_f;
  if (t <= peak_at && peak_at > 0.001f) {
    log_f = log_start + (log_peak - log_start) * (t / peak_at);
  } else {
    const float u = (peak_at >= 0.999f) ? 1.0f : (t - peak_at) / (1.0f - peak_at);
    log_f = log_peak + (log_end - log_peak) * u;
  }

  if (s.vibrato_cents) {
    vib_phase += kTwoPi * s.vibrato_hz * config::VOICE_CONTROL_DIV / (float)rate;
    // Cents over 1200 is octaves, and log2 space is measured in octaves — so
    // the depth applies directly with no conversion.
    log_f += (s.vibrato_cents / 1200.0f) * sinf(vib_phase);
  }

  phase_step = (uint32_t)(exp2f(log_f) * hz_to_phase);

  // Ears are most sensitive between roughly 2 and 5 kHz, so a high syllable at
  // the same amplitude as a low one lands considerably louder and sharper. Back
  // the level off as pitch climbs. Applied here rather than per syllable so it
  // tracks a sweep continuously — a gesture that rises should fade slightly as
  // it does, which is also what a real voice does when it strains.
  float tilt = 1.0f;
  if (log_f > log_tilt_from) {
    float t = (log_f - log_tilt_from) / log_tilt_span;
    if (t > 1.0f) {
      t = 1.0f;
    }
    tilt = 1.0f - t * (config::VOICE_TILT_PCT / 100.0f);
  }

  int32_t env;
  if (pos < attack_len) {
    env = (int32_t)(32767.0f * pos / attack_len);
  } else if (pos + release_len >= seg_len) {
    env = (int32_t)(32767.0f * (seg_len - pos) / release_len);
  } else {
    env = 32767;
  }
  level = (int32_t)(((env * s.level_pct) / 100) * tilt);
}

}  // namespace

void begin(uint32_t sample_rate) {
  rate = sample_rate;
  hz_to_phase = 4294967296.0f / (float)rate;

  log_tilt_from = log2f(config::VOICE_TILT_FROM_HZ);
  log_tilt_span = log2f(config::VOICE_F_MAX) - log_tilt_from;
  if (log_tilt_span <= 0.0f) {
    log_tilt_span = 1.0f;  // guards a config where the taper starts above F_MAX
  }

  for (uint16_t i = 0; i < kTableSize; ++i) {
    sine_table[i] = (int16_t)(32767.0f * sinf(kTwoPi * i / kTableSize));
  }

  playing = false;
  level = 0;
}

void start(const phrase::Phrase& p) {
  if (p.count == 0) {
    return;
  }

  script = p;
  index = 0;
  phase = 0;  // every utterance starts at a zero crossing, so no opening click
  playing = true;
  begin_syllable();
}

void stop() {
  playing = false;
  level = 0;
}

bool speaking() {
  return playing;
}

int16_t next_sample() {
  if (!playing) {
    return 0;
  }

  if (in_gap) {
    if (pos++ >= seg_len) {
      advance();
    }
    return 0;
  }

  if (control_countdown == 0) {
    control_tick();
    control_countdown = config::VOICE_CONTROL_DIV;
  }
  --control_countdown;

  phase += phase_step;

  // Scaling the fundamental down by whatever the harmonic takes keeps the sum
  // inside int16 without a divide or a clamp on the hot path.
  int32_t sample = (osc(phase) * (256 - harmonic)) >> 8;
  if (harmonic) {
    // phase << 1 is 2f, and it wraps correctly for free.
    sample += (osc(phase << 1) * harmonic) >> 8;
  }

  sample = (sample * level) >> 15;

  if (++pos >= seg_len) {
    begin_gap();
  }

  return (int16_t)sample;
}

}  // namespace voice
