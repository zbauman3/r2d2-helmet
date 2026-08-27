# R2-D2 costume helmet

A wearable R2-D2 for Halloween: a painted torso shell plus a dome "hat", with 3D-printed parts holding an RP2040 that makes noises, drives dome lighting, and animates a front logic display.

It is a costume, not a replica. Effects that read well from three metres away in a dim room beat effects that measure well.

## Current state

**Display and both buttons are working on real hardware** as of 2026-08-26 — grid animates, `D2` toggles mode, `D3` fires the flourish. Audio and lights are not wired or written yet.

This also settles the project's one real unknown: the ST7789 comes up clean under arduino-pico with no special handling, contrary to the scattered reports that had it flagged as risky.

`src/` is organised one module per subsystem, each exporting two or three functions with all state private to its translation unit:

| File | Owns |
| --- | --- |
| `config.h` | every pin and hardware constant — nothing else hardcodes a pin |
| `timing.h/.cpp` | `Interval`, the only sanctioned way to do "every N ms" |
| `rng.h/.cpp` | xorshift32 seeded from the ring oscillator |
| `mode.h/.cpp` | standby/active state, owns no hardware |
| `buttons.h/.cpp` | debounce and press-edge detection for both buttons |
| `display.h/.cpp` | ST7789 and the logic-display grid |
| `lights.h/.cpp` | the two dome PSIs on the NeoPixel chain |
| `periscope.h/.cpp` | the SG92R on its hinge |
| `interaction.h/.cpp` | **core0 policy** — which cue fires, and agitation |
| `cue.h` | the 32-bit word that is the entire core0↔core1 contract |
| `audio.h/.cpp` | **core1** — owns PWMAudio, drains cues, pumps samples |
| `phrase.h/.cpp` | **core1** — composes an utterance from a cue |
| `voice.h/.cpp` | **core1** — renders a phrase to samples |
| `main.cpp` | the only place that decides what a button press *means* |

### Core split

The one rule that keeps this simple: **the only thing crossing between cores is a 32-bit cue word through the hardware FIFO.** No shared structs, no locks.

| | Owns |
| --- | --- |
| **core0** | mode, buttons, display, lights, interaction, `rng::state[0]`, `Serial` |
| **core1** | audio, phrase, voice, `rng::state[1]` |

If core1 needs to know something core0 knows, add a cue for it — do not reach across. `rng` keeps per-core state because a shared generator would be a read-modify-write race, and Cortex-M0+ has no exclusive load/store to make it safe.

Still to write: nothing structural. Remaining work is tuning.

The two dome pixels are R2's **Process State Indicators** — a lens on the front of the dome below the logic displays and a matching one on the back. Canonical colour pairs: **front alternates red/blue, rear alternates green/yellow.** Each runs an independent randomised rhythm so they drift in and out of phase; two pixels sharing a period reads as one circuit driving both.

## Features

| Feature | Status |
| --- | --- |
| Synthesized R2 voice through a mono speaker | code written, untested on hardware |
| Front logic display — coarse block grid on a 1.9" TFT | **working on hardware** |
| Dome lighting — front and rear PSI | code written, untested on hardware |
| Mode button — toggles "active" on and off | **working on hardware** |
| Trigger button — fires an interaction on demand | screen and lights working, sound written |
| Pattern logic selecting *which* interaction fires | written — agitation + no-repeat |
| Periscope servo | code written, untested — **needs the `USB+ → RAW` jumper closed** |
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
