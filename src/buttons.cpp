#include "buttons.h"

#include "config.h"

namespace buttons {
namespace {

struct Debounced {
  uint8_t pin;
  bool pressed;        // committed state, after debouncing
  bool last_raw;       // most recent raw read, bouncing and all
  uint32_t settled_ms; // when last_raw last changed
};

Debounced mode_btn{config::PIN_BTN_MODE, false, false, 0};
Debounced trigger_btn{config::PIN_BTN_TRIGGER, false, false, 0};

// Returns true on the press edge only. Releases are swallowed: nothing in this
// design cares about them, and reporting both would make every call site
// filter for the one it wants.
bool poll(Debounced& b, uint32_t now_ms) {
  const bool raw = (digitalRead(b.pin) == LOW);

  // Any change restarts the settling window, so a burst of chatter keeps
  // pushing the deadline out and only the final resting state is committed.
  if (raw != b.last_raw) {
    b.last_raw = raw;
    b.settled_ms = now_ms;
    return false;
  }

  if (raw == b.pressed) {
    return false;
  }

  if ((uint32_t)(now_ms - b.settled_ms) < config::BUTTON_DEBOUNCE_MS) {
    return false;
  }

  b.pressed = raw;
  return b.pressed;
}

// Seed from the real pin state instead of assuming released. A button held down
// at power-on would otherwise register as a press the moment it is let go, which
// is exactly backwards.
void seed(Debounced& b) {
  b.last_raw = (digitalRead(b.pin) == LOW);
  b.pressed = b.last_raw;
  b.settled_ms = millis();
}

}  // namespace

void begin() {
  pinMode(mode_btn.pin, INPUT_PULLUP);
  pinMode(trigger_btn.pin, INPUT_PULLUP);

  // Both pull-ups are enabled before either pin is read. They have to charge the
  // pin's stray capacitance, and on a long unshielded run out to a button buried
  // in the costume shell that is not instant — reading too early latches a
  // phantom press that only clears on the first real one.
  delay(1);

  seed(mode_btn);
  seed(trigger_btn);
}

Events update(uint32_t now_ms) {
  Events e;
  e.mode_pressed = poll(mode_btn, now_ms);
  e.trigger_pressed = poll(trigger_btn, now_ms);
  return e;
}

}  // namespace buttons
