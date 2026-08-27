#include "audio.h"

#include <PWMAudio.h>

#include "config.h"
#include "phrase.h"
#include "sample.h"
#include "voice.h"

// The recorded scream is an OPTIONAL, untracked asset — see .gitignore. Absent,
// the build falls back to the synthesised Scream recipe in phrase.cpp, so a
// fresh clone compiles and works without it.
#if defined(__has_include)
#if __has_include("scream_pcm.h")
#include "scream_pcm.h"
#define R2_HAVE_SCREAM_SAMPLE 1
#endif
#endif
#ifndef R2_HAVE_SCREAM_SAMPLE
#define R2_HAVE_SCREAM_SAMPLE 0
#endif

#if R2_HAVE_SCREAM_SAMPLE
static_assert(SCREAM_PCM_RATE == config::AUDIO_SAMPLE_RATE,
              "the scream sample must already be at the output sample rate — "
              "there is no resampling in sample.cpp");
#endif

namespace audio {
namespace {

PWMAudio pcm(config::PIN_AUDIO_PWM, /*stereo=*/false);

// Core1-private. Kept as a file static rather than a local so that composing a
// phrase never touches the stack in a way that would matter on the audio path.
phrase::Phrase pending;

bool running = false;

// Which generator is feeding the DAC. Only one is ever live: a scream replaces
// an utterance outright rather than mixing with it.
enum class Source : uint8_t { Voice, Sample };
Source source = Source::Voice;

int16_t next_sample() {
  return (source == Source::Sample) ? sample::next_sample() : voice::next_sample();
}

bool generating() {
  return (source == Source::Sample) ? sample::playing() : voice::speaking();
}

// core1: tracks the speaking edge so the finish is reported exactly once.
bool was_speaking = false;

// core0: mirrors core1's state. Owned by core0 alone — core1 never reads it.
bool core0_busy = false;
uint32_t busy_since_ms = 0;

void handle(uint32_t word) {
  const cue::Kind kind = cue::kind_of(word);

  if (kind == cue::Kind::Stop || kind == cue::Kind::None) {
    voice::stop();
    sample::stop();
    return;
  }

#if R2_HAVE_SCREAM_SAMPLE
  if (kind == cue::Kind::Scream) {
    // Recorded, not synthesised. Stop the other source explicitly rather than
    // relying on the switch — otherwise a half-finished utterance would resume
    // mid-word the next time the source flipped back.
    voice::stop();
    sample::start(SCREAM_PCM, SCREAM_PCM_LEN);
    source = Source::Sample;
    return;
  }
#endif

  // Everything else, and the scream too when the sample is not present.
  sample::stop();
  phrase::compose(pending, kind, cue::agitation_of(word));
  voice::start(pending);
  source = Source::Voice;

  // Arm the finish edge here rather than relying on the pump to observe
  // speaking() go true. An utterance short enough to be generated entirely
  // within one service() call would otherwise never present a rising edge, no
  // Finished reply would be sent, and core0 would consider the voice busy until
  // the safety timeout expired.
  was_speaking = true;
}

}  // namespace

void begin() {
  voice::begin(config::AUDIO_SAMPLE_RATE);

  pcm.setBuffers(config::AUDIO_BUFFERS, config::AUDIO_BUFFER_WORDS);

  // Carrier is set explicitly. PWMAudio's constructor defaults it to 48 kHz —
  // barely above the audio band, and it would sit right on top of everything
  // the RC filter is meant to remove.
  running = pcm.begin(config::AUDIO_SAMPLE_RATE, config::AUDIO_CARRIER_HZ);
}

void service() {
  if (!running) {
    return;
  }

  // Drain rather than handle one per pass. Two presses landing between service
  // calls should not leave a stale cue sitting in the FIFO for the next one.
  uint32_t word;
  while (rp2040.fifo.pop_nb(&word)) {
    handle(word);
  }

  // Fill unconditionally, including with silence when idle. Feeding explicit
  // zeros rather than letting the buffer run dry means the DMA never hits an
  // underflow, so there is no discontinuity to hear at the start or end of an
  // utterance. In mono each int16 consumes one 32-bit word, and
  // availableForWrite() reports bytes — hence the 4.
  while (pcm.availableForWrite() >= 4) {
    pcm.write(next_sample(), /*sync=*/false);
  }

  // Report the finish once, on the edge.
  //
  // This fires when the last sample has been GENERATED, which is up to a buffer
  // ahead of when it is actually heard — roughly 46 ms. That is deliberate and
  // useful: a follow-up utterance queued here starts generating just as the
  // previous one drains, so the two butt together seamlessly instead of leaving
  // a buffer-length hole between them.
  const bool speaking = generating();
  if (was_speaking && !speaking) {
    rp2040.fifo.push_nb(cue::pack_reply(cue::Reply::Finished));
  }
  was_speaking = speaking;
}

bool request(cue::Kind kind, uint8_t agitation) {
  if (!rp2040.fifo.push_nb(cue::pack(kind, agitation))) {
    return false;
  }

  // Assume busy from the moment the cue is accepted rather than waiting for
  // core1 to acknowledge. Core1 may not run for a millisecond or two, and a
  // window where core0 believes the voice is idle when a cue is already in
  // flight is exactly how a chirp gets cut off.
  core0_busy = true;
  busy_since_ms = millis();
  return true;
}

void poll(uint32_t now_ms) {
  uint32_t word;
  while (rp2040.fifo.pop_nb(&word)) {
    if (cue::reply_of(word) == cue::Reply::Finished) {
      core0_busy = false;
    }
  }

  // A lost reply would otherwise silence the costume for the rest of the
  // evening, which is a bad failure mode for a defect nobody would notice in
  // testing.
  if (core0_busy &&
      (uint32_t)(now_ms - busy_since_ms) > config::AUDIO_BUSY_TIMEOUT_MS) {
    core0_busy = false;
  }
}

bool busy(uint32_t now_ms) {
  (void)now_ms;
  return core0_busy;
}

}  // namespace audio
