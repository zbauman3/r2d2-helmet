#include "mode.h"

namespace mode {
namespace {

Mode current = Mode::Standby;  // powers on dark; nobody wants a costume that
                               // starts talking the moment it is plugged in
uint32_t changed_ms = 0;

}  // namespace

void begin() {
  current = Mode::Standby;
  changed_ms = millis();
}

Mode get() {
  return current;
}

void set(Mode m) {
  // Guarding on an actual change keeps changed_at_ms() meaningful — consumers
  // use it to run transition animations, which should not re-fire when the
  // button is pressed twice into the same state.
  if (m == current) {
    return;
  }

  current = m;
  changed_ms = millis();
}

Mode toggle() {
  set(current == Mode::Active ? Mode::Standby : Mode::Active);
  return current;
}

uint32_t changed_at_ms() {
  return changed_ms;
}

}  // namespace mode
