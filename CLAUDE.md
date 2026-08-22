# R2-D2 costume helmet

A wearable R2-D2 for Halloween: a painted torso shell plus a dome "hat", with 3D-printed parts holding an RP2040 that makes noises, drives dome lighting, and animates a front logic display.

It is a costume, not a replica. Effects that read well from three metres away in a dim room beat effects that measure well.

## Current state

Wiring is designed and documented but **not yet built**. No project software exists yet — `src/main.cpp` is still PlatformIO boilerplate. The sequence is: wire it, bring up the display, then audio, then lights.

## Features

| Feature | Status |
| --- | --- |
| Synthesized R2 voice through a mono speaker | designed, not built |
| Front logic display — coarse block grid on a 1.9" TFT | designed, not built |
| Dome lighting — 2 NeoPixels behind a diffuser | designed, not built |
| Mode button — toggles "active" on and off | designed, not built |
| Trigger button — fires an interaction on demand | designed, not built |
| Pattern logic selecting *which* interaction fires | not designed |
| Periscope servo | deferred, GP6 reserved, needs a power rework first |
| Restraining-bolt "wake up" gag (magnet + hall sensor) | open, unbought |
| Cosmetic electro-shock prod | open, non-electronic |

## Behaviour model

Two hidden buttons, two jobs.

**Button 1 — mode**, on `D2`/GP2. Toggles between **standby** and **active**. In active mode the screen animates, the dome lights animate, and the periscope moves when it exists. **Active mode is deliberately silent.** In standby all of that is dark and still.

**Button 2 — trigger**, on `D3`/GP3. Fires a one-off interaction: a sound, plus whatever screen and light flourish goes with it. This is the only thing in the design that makes noise. It works in **both** modes, so you can bleep at someone without lighting up first — that's a taste call and trivial to reverse.

Which interaction fires is **pattern-driven, and the patterns are not designed yet**. Assume something more structured than uniform-random selection. See the `r2-sounds` skill.

This retires the earlier open question about autonomous idle chirping. The liveliness comes from active mode's movement and light; sound stays manual and deliberate, which is also kinder to everyone standing nearby for an evening.

## Hard rules

Violating any of these breaks the build in a way that is expensive to diagnose:

1. **We use Arduino.** Specifically earlephilhower's `arduino-pico` core, which is what provides `setup1()`/`loop1()`. The core we rejected is Arduino's official *Mbed* core (EOL July 2026). Arduino libraries are expected and fine.
2. **PlatformIO must use maxgerhardt's platform fork.** The official `platform-raspberrypi` ships only the Mbed core. See the `firmware` skill.
3. **`PWMAudio` runs entirely on core1.** Core0 owns display, pixels, and logic. Audio synthesis must never live in `loop()`.
4. **Never set the PWM carrier to ~260 kHz.** It beats against the amplifier's 250 kHz switching at 10 kHz, in the middle of the audio band.
5. **NeoPixels are powered from `3V`, not `RAW`.** This is what makes 3.3 V logic drive them without a level shifter.
6. **Master volume is the trim pot, not software.** The PWM DAC has 10 bits; attenuating in code throws them away.
7. **No microSD.** Sounds are synthesized, the display draws primitives.

## Where things live

- `README.md` — everything needed with a soldering iron in hand: BOM, board geometry, the mermaid schematic, and the connection tables to wire from.
- `.claude/skills/hardware/` — pin map, power budget, and the electrical reasoning behind the wiring.
- `.claude/skills/firmware/` — PlatformIO, the core, build/flash, concurrency model.
- `.claude/skills/r2-sounds/` — how the voice was actually made, and how to synthesize it here.
- `.claude/skills/bring-up/` — first-power-on order and failure signatures.

Facts live in exactly one of those files. If something needs saying in two places, one of them should link instead of restating.
