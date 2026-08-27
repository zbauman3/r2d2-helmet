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

// D2 gesture state. A "gesture" runs from the first press until the multi-click
// gap expires with the button up, or until a hold consumes it.
uint8_t press_count = 0;
uint32_t press_started_ms = 0;
uint32_t released_ms = 0;
bool hold_consumed = false;
bool mode_down = false;  // previous debounced state, for release-edge detection

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

  press_count = 0;
  hold_consumed = false;
  mode_down = mode_btn.pressed;
  released_ms = millis();
}

Events update(uint32_t now_ms) {
  Events e;

  // --- D3: immediate ---
  e.trigger_pressed = poll(trigger_btn, now_ms);

  // --- D2: counted, with a hold escape ---
  if (poll(mode_btn, now_ms)) {
    ++press_count;
    press_started_ms = now_ms;
  }

  // Latch the RELEASE EDGE. This has to be an edge, not a level test: stamping
  // released_ms on every pass while the button happens to be up keeps the gap
  // timer permanently at zero, so it never expires, press_count is never
  // cleared, and hold_consumed never clears either — after one hold, D2 is
  // dead for the rest of the session. That was a real bug, not a hypothetical.
  const bool released_now = mode_down && !mode_btn.pressed;
  mode_down = mode_btn.pressed;

  if (released_now) {
    released_ms = now_ms;

    // A hold has already fired its action, so there is nothing left to resolve.
    // End the gesture on release instead of making the user wait out the
    // multi-click gap before D2 answers to anything again.
    if (hold_consumed) {
      press_count = 0;
      hold_consumed = false;
    }
  }

  if (mode_btn.pressed && press_count == 1 && !hold_consumed &&
      (uint32_t)(now_ms - press_started_ms) >= config::BUTTON_HOLD_MS) {
    // Fires while the finger is still down rather than waiting for release, so
    // a hold feels like it lands at the moment it is recognised. Only armed on
    // the FIRST press of a gesture: holding the second press of a double is
    // ambiguous, and resolving it as a count is the less surprising reading.
    e.mode_toggle = true;
    hold_consumed = true;
  }

  if (!mode_btn.pressed && press_count > 0 &&
      (uint32_t)(now_ms - released_ms) >= config::BUTTON_MULTI_GAP_MS) {
    // 1 and 4 are both dead by design. 1 absorbs an accidental brush; 4 is a
    // deliberate buffer between "yes" at three and the scream at five, so
    // overshooting a triple by one press does nothing rather than shrieking.
    // 6 or more is a fumble, and doing nothing beats guessing.
    if (press_count == 2) {
      e.answer_no = true;
    } else if (press_count == 3) {
      e.answer_yes = true;
    } else if (press_count == 5) {
      e.scream = true;
    }
    press_count = 0;
  }

  return e;
}

}  // namespace buttons
