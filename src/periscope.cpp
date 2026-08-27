#include "periscope.h"

#include <Servo.h>

#include "arousal.h"
#include "config.h"
#include "mode.h"
#include "mood.h"
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

// --- Scripted gestures -----------------------------------------------------
//
// A nod or a dip is a fixed sequence, not a wander, so it runs from a little
// script that overrides target selection until it finishes. Everything is
// written in terms of UP and DOWN rather than raw angles, so a mirrored hinge
// is fixed by swapping two constants in config.h and nothing else.

constexpr uint8_t MAX_STEPS = 8;

struct GestureStep {
  int16_t angle;
  uint16_t hold_ms;
  uint8_t dps;
};

GestureStep script[MAX_STEPS];
uint8_t script_len = 0;
uint8_t script_at = 0;
bool in_gesture = false;

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
  const uint16_t dwell = arousal::pace(config::PERISCOPE_DWELL_CALM_MS,
                                       config::PERISCOPE_DWELL_HOT_MS, now_ms);
  slew_dps = arousal::pace(config::PERISCOPE_SLEW_CALM_DPS,
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

// Signed offset toward "up", regardless of which end of the travel that is.
int16_t toward_up(int16_t from, int16_t degrees) {
  const bool up_is_high = config::PERISCOPE_ANGLE_UP > config::PERISCOPE_ANGLE_DOWN;
  return clamp_angle(from + (up_is_high ? degrees : -degrees));
}

void run_script(uint32_t now_ms) {
  in_gesture = true;
  script_at = 0;
  target = script[0].angle;
  slew_dps = script[0].dps;
  moving = true;
  dwell_until_ms = now_ms + script[0].hold_ms;
  step.reset(now_ms);
}

// Yes: nod up and down about a centre, ending back where it started. Slower
// than a flourish on purpose — it has to read as an answer, not a twitch.
void build_nod(uint32_t now_ms) {
  const int16_t centre = clamp_angle(angle_tenths / 10);
  const int16_t up = toward_up(centre, config::PERISCOPE_NOD_DEGREES / 2);
  const int16_t down = toward_up(centre, -(config::PERISCOPE_NOD_DEGREES / 2));

  script_len = 0;
  for (uint8_t i = 0; i < config::PERISCOPE_NOD_COUNT && script_len + 2 <= MAX_STEPS; ++i) {
    script[script_len++] = {up, config::PERISCOPE_NOD_HOLD_MS, config::PERISCOPE_NOD_DPS};
    script[script_len++] = {down, config::PERISCOPE_NOD_HOLD_MS, config::PERISCOPE_NOD_DPS};
  }
  if (script_len < MAX_STEPS) {
    script[script_len++] = {centre, 0, config::PERISCOPE_NOD_DPS};
  }

  run_script(now_ms);
}

// No: drop decisively, sag, and come back up only reluctantly. The hinge moves
// in one axis only, so a head-shake is not available — a fast fall and a slow
// recovery is the closest this mechanism gets to refusal.
void build_dip(uint32_t now_ms) {
  const int16_t here = clamp_angle(angle_tenths / 10);
  const int16_t low = toward_up(here, -(int16_t)config::PERISCOPE_DIP_DEGREES);
  const int16_t sag = toward_up(low, config::PERISCOPE_DIP_DEGREES / 4);

  script_len = 0;
  script[script_len++] = {low, config::PERISCOPE_DIP_HOLD_MS, config::PERISCOPE_DIP_DPS};
  script[script_len++] = {sag, config::PERISCOPE_DIP_HOLD_MS, config::PERISCOPE_DIP_DPS / 2};
  script[script_len++] = {low, config::PERISCOPE_DIP_HOLD_MS * 2, config::PERISCOPE_DIP_DPS};
  // Deliberately slow on the way back — the reluctance is the message.
  script[script_len++] = {here, 0, config::PERISCOPE_NOD_DPS / 2};

  run_script(now_ms);
}

// Waking up: lift the head out of park, overshoot slightly, settle. The settle
// is what makes it read as something coming to rather than a motor being told
// to go somewhere.
void build_wake(uint32_t now_ms) {
  const int16_t here = clamp_angle(angle_tenths / 10);
  const int16_t top = toward_up(here, config::PERISCOPE_WAKE_RISE);
  const int16_t settled = toward_up(top, -(int16_t)config::PERISCOPE_WAKE_SETTLE);

  script_len = 0;
  script[script_len++] = {top, config::PERISCOPE_WAKE_HOLD_MS,
                          config::PERISCOPE_WAKE_DPS};
  script[script_len++] = {settled, 0,
                          (uint8_t)(config::PERISCOPE_WAKE_DPS / 2)};

  run_script(now_ms);
}

// Going to sleep: one last small lift, then a slow sag into park. Slow on
// purpose — tempo is the difference between this and the wake, and that
// difference is the whole signal to someone who cannot see the dome.
void build_sleep(uint32_t now_ms) {
  const int16_t here = clamp_angle(angle_tenths / 10);
  const int16_t lift = toward_up(here, config::PERISCOPE_SLEEP_LIFT);

  script_len = 0;
  script[script_len++] = {lift, config::PERISCOPE_SLEEP_HOLD_MS,
                          config::PERISCOPE_SLEEP_DPS};
  script[script_len++] = {(int16_t)config::PERISCOPE_PARK, 0,
                          config::PERISCOPE_SLEEP_DPS};

  run_script(now_ms);
}

void on_mode_change(mode::Mode m, uint32_t now_ms) {
  in_gesture = false;

  if (m == mode::Mode::Active) {
    attach();
    focus = (config::PERISCOPE_ANGLE_MIN + config::PERISCOPE_ANGLE_MAX) / 2;
    was_large_move = false;
    build_wake(now_ms);
  } else {
    // Must stay attached to move — release() happens at the end of the stow
    // script, not here, or the servo would go limp before it had parked.
    attach();
    build_sleep(now_ms);
  }

  last_mode = m;
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
  step.reset(millis());
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

  // Arrived at the current target.
  if ((int32_t)(now_ms - dwell_until_ms) < 0) {
    return;
  }

  // A running script outranks the standby release: the stow gesture has to
  // finish before the servo is allowed to go limp, or it would sag halfway
  // through instead of settling into park.
  if (in_gesture) {
    ++script_at;
    if (script_at < script_len) {
      target = script[script_at].angle;
      slew_dps = script[script_at].dps;
      dwell_until_ms = now_ms + script[script_at].hold_ms;
      moving = true;
      return;
    }

    in_gesture = false;

    if (m != mode::Mode::Active) {
      release();
      return;
    }

    // Fold the finish back into normal scanning from wherever it ended up,
    // rather than snapping back to the old focus.
    focus = clamp_angle(angle_tenths / 10);
    was_large_move = false;
    choose_target(now_ms);
    return;
  }

  if (m != mode::Mode::Active) {
    release();
    return;
  }

  choose_target(now_ms);
}

void answer(bool yes, uint32_t now_ms) {
  if (mode::get() != mode::Mode::Active) {
    return;
  }

  attach();
  if (yes) {
    build_nod(now_ms);
  } else {
    build_dip(now_ms);
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

  in_gesture = false;  // a trigger interrupts an answer in progress
  focus = clamp_angle(next);
  target = focus;
  moving = true;
  was_large_move = true;
  slew_dps = config::PERISCOPE_SLEW_HOT_DPS;
  dwell_until_ms = now_ms + config::PERISCOPE_DWELL_HOT_MS;
  step.reset(now_ms);
}

}  // namespace periscope
