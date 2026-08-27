#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Top-level behaviour state.
//
// Standby: dark and still. Active: screen, lights and (eventually) the
// periscope animate — and stay SILENT. Sound is only ever the trigger button,
// which fires in either mode. See CLAUDE.md for the full behaviour model.
//
// This module owns no hardware. It is the one piece of shared state the display,
// lights and sound modules all read, so it deliberately has no dependencies of
// its own beyond Arduino.
// ---------------------------------------------------------------------------

namespace mode {

enum class Mode : uint8_t {
  Standby,
  Active,
};

void begin();

Mode get();

void set(Mode m);

// Returns the mode after toggling, so callers can log or react without a
// second get().
Mode toggle();

// millis() at the last change. Consumers compare against their own last-seen
// value rather than this module offering a consume-once flag: there are several
// consumers, and a flag that the first reader clears is a bug waiting to happen.
uint32_t changed_at_ms();

}  // namespace mode
