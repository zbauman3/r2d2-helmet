#include "sample.h"

namespace sample {
namespace {

const int16_t* data = nullptr;
uint32_t length = 0;
uint32_t at = 0;

}  // namespace

void start(const int16_t* pcm, uint32_t count) {
  if (pcm == nullptr || count == 0) {
    return;
  }
  data = pcm;
  length = count;
  at = 0;
}

void stop() {
  at = length;
}

bool playing() {
  return data != nullptr && at < length;
}

int16_t next_sample() {
  if (!playing()) {
    return 0;
  }

  // A straight flash read. RP2040 executes in place through a cache, and a
  // linear 22 kHz stream is about 44 KB/s — nothing the XIP cache notices.
  return data[at++];
}

}  // namespace sample
