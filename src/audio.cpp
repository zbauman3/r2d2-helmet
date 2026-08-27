#include "audio.h"

#include <PWMAudio.h>

#include "config.h"
#include "phrase.h"
#include "voice.h"

namespace audio {
namespace {

PWMAudio pcm(config::PIN_AUDIO_PWM, /*stereo=*/false);

// Core1-private. Kept as a file static rather than a local so that composing a
// phrase never touches the stack in a way that would matter on the audio path.
phrase::Phrase pending;

bool running = false;

void handle(uint32_t word) {
  const cue::Kind kind = cue::kind_of(word);

  if (kind == cue::Kind::Stop || kind == cue::Kind::None) {
    voice::stop();
    return;
  }

  phrase::compose(pending, kind, cue::agitation_of(word));
  voice::start(pending);
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
    pcm.write(voice::next_sample(), /*sync=*/false);
  }
}

bool request(cue::Kind kind, uint8_t agitation) {
  return rp2040.fifo.push_nb(cue::pack(kind, agitation));
}

}  // namespace audio
