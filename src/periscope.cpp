#include "periscope.h"

#include <Servo.h>

#include "config.h"
#include "interaction.h"
#include "mode.h"
#include "rng.h"
#include "timing.h"

namespace periscope {
namespace {

Servo servo;

// Commanded angle, tracked in tenths of a degree. The servo only resolves whole
// degrees, but slewing at 65 deg/s on a 20 ms tick is 1.3 degrees per step —
// rounding that to integers every step would quantise the motion into a visible
// stutter and accumulate error.
int16_t angle_tenths = config::PERISCOPE_PARK * 10;
int16_t target = config::PERISCOPE_PARK;
uint8_t written = 255;  // last angle actually sent; 255 = nothing yet

// Where R2 is currently "looking". Most targets are small excursions from here.
int16_t focus = (config::PERISCOPE_ANGLE_MIN + config::PERISCOPE_ANGLE_MAX) / 2;

bool attached = false;
bool moving = false;
bool was_large_move = false;

uint32_t dwell_until_ms = 0;
uint16_t slew_dps = config::PERISCOPE_SLEW_CALM_DPS;

timing::Interval step(config::PERISCOPE_STEP_MS);
mode::Mode last_mode = mode::Mode::Standby;

int16_t clamp_angle(int16_t a) {
  return constrain(a, (int16_t)config::PERISCOPE_ANGLE_MIN,
                   (int16_t)config::PERISCOPE_ANGLE_MAX);
}

void attach() {
  if (attached) {
    return;
  }

  const uint8_t here = (uint8_t)clamp_angle(angle_tenths / 10);

  // The fourth argument matters as much as the pulse range does. The 3-argument
  // attach() forwards _valueUs, which the constructor sets to
  // DEFAULT_NEUTRAL_PULSE_WIDTH (1500 us) — dead centre of our 500-2500 range,
  // i.e. 90 degrees. Attaching that way commands a full-speed slam from wherever
  // the horn physically sits to mid-travel, through whatever the linkage is
  // attached to. Passing the angle we already believe we are at makes attaching
  // a no-op instead of an event.
  servo.attach(config::PIN_SERVO, config::SERVO_MIN_US, config::SERVO_MAX_US, here);

  written = here;
  attached = true;
}

void release() {
  if (!attached) {
    return;
  }
  // Detaching stops the pulse train, which stops the servo holding position.
  // Worth doing in standby: a small servo under load hums continuously while
  // holding, and a costume that buzzes at rest is a costume nobody wants to
  // stand next to for an evening.
  servo.detach();
  attached = false;
}

void command(int16_t degrees) {
  const uint8_t a = (uint8_t)clamp_angle(degrees);
  if (a == written) {
    return;
  }
  servo.write(a);
  written = a;
}

// Chooses somewhere to look next. The shape of this function is the whole
// difference between "alive" and "twitching".
void choose_target(uint32_t now_ms) {
  const uint16_t dwell = interaction::pace(config::PERISCOPE_DWELL_CALM_MS,
                                           config::PERISCOPE_DWELL_HOT_MS, now_ms);
  slew_dps = interaction::pace(config::PERISCOPE_SLEW_CALM_DPS,
                               config::PERISCOPE_SLEW_HOT_DPS, now_ms);

  // Stillness is a behaviour, not an absence of one. Holding position reads as
  // attention — something has his interest — and it gives the movements that
  // follow somewhere to depart from.
  if (!was_large_move && rng::chance(config::PERISCOPE_HOLD_PCT)) {
    dwell_until_ms = now_ms + dwell;
    moving = false;
    return;
  }

  // A double-take: having swung somewhere new, come back for a second look
  // shortly after. Cheap, and it is the single most convincingly deliberate
  // thing this module does.
  if (was_large_move && rng::chance(config::PERISCOPE_RECHECK_PCT)) {
    target = clamp_angle(focus + (int16_t)rng::below(13) - 6);
    dwell_until_ms = now_ms + dwell / 3;
    was_large_move = false;
    moving = true;
    return;
  }

  if (rng::chance(config::PERISCOPE_REFOCUS_PCT)) {
    // Something else caught his attention: pick a genuinely new region.
    focus = clamp_angle(config::PERISCOPE_ANGLE_MIN +
                        rng::below(config::PERISCOPE_ANGLE_MAX -
                                   config::PERISCOPE_ANGLE_MIN));
    target = focus;
    was_large_move = true;
  } else {
    // Scanning around whatever he is already looking at.
    const int16_t spread = config::PERISCOPE_SCAN_SPREAD;
    target = clamp_angle(focus + (int16_t)rng::below(spread * 2 + 1) - spread);
    was_large_move = false;
  }

  dwell_until_ms = now_ms + dwell;
  moving = true;
}

void on_mode_change(mode::Mode m, uint32_t now_ms) {
  if (m == mode::Mode::Active) {
    attach();
    focus = (config::PERISCOPE_ANGLE_MIN + config::PERISCOPE_ANGLE_MAX) / 2;
    was_large_move = false;
    choose_target(now_ms);
  } else {
    // Stow, then let go. The park move still slews rather than snapping, so it
    // is handled by the normal path; release() happens once it arrives.
    target = config::PERISCOPE_PARK;
    moving = true;
    dwell_until_ms = now_ms;
    slew_dps = config::PERISCOPE_SLEW_CALM_DPS;
  }

  last_mode = m;
  step.reset(now_ms);
}

}  // namespace

void begin() {
  // Start from the parked position with nothing driven. Attaching in standby
  // would make the servo snap to wherever the library's neutral happens to be,
  // which on a hinged periscope means slamming it before anyone has asked for
  // anything.
  angle_tenths = config::PERISCOPE_PARK * 10;
  target = config::PERISCOPE_PARK;
  written = 255;
  attached = false;
  moving = false;

  last_mode = mode::get();
  if (last_mode == mode::Mode::Active) {
    on_mode_change(last_mode, millis());
  }
}

void update(uint32_t now_ms) {
  const mode::Mode m = mode::get();
  if (m != last_mode) {
    on_mode_change(m, now_ms);
  }

  if (!step.due(now_ms)) {
    return;
  }

  const int16_t goal_tenths = target * 10;

  if (angle_tenths != goal_tenths) {
    // Degrees-per-second over a 20 ms tick, in tenths.
    const int16_t stride =
        max<int16_t>((int16_t)((slew_dps * config::PERISCOPE_STEP_MS) / 100), 1);

    if (abs(goal_tenths - angle_tenths) <= stride) {
      angle_tenths = goal_tenths;
    } else {
      angle_tenths += (goal_tenths > angle_tenths) ? stride : -stride;
    }

    if (attached) {
      command(angle_tenths / 10);
    }
    return;
  }

  // Arrived. In standby that means the stow is finished and the servo can stop
  // holding; in active it means it is time to decide where to look next.
  if (m != mode::Mode::Active) {
    release();
    return;
  }

  if ((int32_t)(now_ms - dwell_until_ms) >= 0) {
    choose_target(now_ms);
  }
}

void flourish(uint32_t now_ms) {
  if (mode::get() != mode::Mode::Active) {
    return;
  }

  attach();

  // A trigger is something happening, so look at it: a decisive move well away
  // from wherever he was, at the agitated slew rate regardless of how calm he
  // was a moment ago.
  const int16_t span = config::PERISCOPE_ANGLE_MAX - config::PERISCOPE_ANGLE_MIN;
  int16_t next = config::PERISCOPE_ANGLE_MIN + (int16_t)rng::below(span);

  // Reject a target that is barely a move — a flourish nobody can see is not a
  // reaction.
  if (abs(next - (angle_tenths / 10)) < span / 4) {
    next = (next > (config::PERISCOPE_ANGLE_MIN + span / 2))
               ? config::PERISCOPE_ANGLE_MIN + span / 6
               : config::PERISCOPE_ANGLE_MAX - span / 6;
  }

  focus = clamp_angle(next);
  target = focus;
  moving = true;
  was_large_move = true;
  slew_dps = config::PERISCOPE_SLEW_HOT_DPS;
  dwell_until_ms = now_ms + config::PERISCOPE_DWELL_HOT_MS;
  step.reset(now_ms);
}

}  // namespace periscope
