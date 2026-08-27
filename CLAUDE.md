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
| `mood.h/.cpp` | the short window a deliberate yes/no colours everything |
| `arousal.h/.cpp` | **the shared tempo metric** — how wound up he is, 0–255 |
| `buttons.h/.cpp` | debounce and press-edge detection for both buttons |
| `display.h/.cpp` | ST7789 and the logic-display grid |
| `lights.h/.cpp` | the two dome PSIs on the NeoPixel chain |
| `periscope.h/.cpp` | the SG92R on its hinge |
| `interaction.h/.cpp` | **core0 policy** — which cue fires |
| `cue.h` | the 32-bit word that is the entire core0↔core1 contract |
| `audio.h/.cpp` | **core1** — owns PWMAudio, drains cues, pumps samples |
| `phrase.h/.cpp` | **core1** — composes an utterance from a cue |
| `voice.h/.cpp` | **core1** — renders a phrase to samples |
| `sample.h/.cpp` | **core1** — plays recorded audio from flash |
| `scream_pcm.*` | **generated** — the scream, 97 KB of PCM in flash |
| `main.cpp` | the only place that decides what a button press *means* |

### Core split

The one rule that keeps this simple: **the cores only ever exchange 32-bit words through the hardware FIFO.** No shared structs, no locks. Core0 sends cues; core1 sends back a `Finished` reply when an utterance is done, which is what lets core0 throttle presses without ever asking core1 a question.

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
| Deliberate yes / no answers across all four subsystems | code written, untested on hardware |
| Pattern logic selecting *which* interaction fires | written — agitation + no-repeat |
| Periscope servo | code written, untested on hardware |
| Restraining-bolt "wake up" gag (magnet + hall sensor) | open, unbought |
| Cosmetic electro-shock prod | open, non-electronic |

## Behaviour model

Two hidden buttons. `D3` is immediate and expressive; `D2` is counted and deliberate.

**`D2`/GP2 is counted, and a single press is DEAD.** That is deliberate — the button can be brushed by accident inside a costume without doing anything.

| Gesture | Does |
| --- | --- |
| one press | **nothing** |
| double press | **no** — falling chirp, red PSIs, warm screen, periscope drops and recovers slowly |
| triple press | **yes** — rising chirp, green PSIs, cool screen, periscope nods |
| four presses | **nothing** — a deliberate buffer, so overshooting a triple does not shriek |
| five presses | **scream** — pins arousal at maximum, whole costume goes frantic |
| hold 1 s | toggle **standby** / **active** |
| six or more | nothing — a fumble is safer ignored than guessed at |

Presses count as one gesture while the gap between them stays under `BUTTON_MULTI_GAP_MS`.

Counting works here precisely *because* a single press does nothing: with no immediate action to delay, waiting to see whether more presses arrive costs no latency. The same scheme on `D3` would push every chirp back by the width of the window, which is why `D3` is a plain tap.

**`D3`/GP3 — trigger.** Fires a one-off interaction: a sound plus the screen, light and periscope flourish that goes with it. Repeated presses escalate agitation. It works in **both** modes, so you can bleep at someone without lighting up first.

**Entering and leaving active mode is announced by the periscope**, because the wearer cannot see the dome. Active runs a brisk lift out of park with a small settle; standby runs a slow lift-and-sag into park. The difference in tempo is the signal — it says which way the toggle went, not merely that something moved.

In active mode the screen animates, the dome lights animate and the periscope scans. **Active mode is deliberately silent** — sound only ever comes from `D3` or an answer. In standby all of that is dark and still.

**Answers do not touch arousal.** The current level still colours their delivery, but they neither escalate it nor reset the exchange — an answer is communication, not frustration.

### Arousal is the one shared tempo metric

`arousal` (0–255) rises with rapid `D3` presses and decays on its own. Every subsystem with a rhythm reads it through `arousal::pace(calm, agitated, now)`:

| | calm | wound up |
| --- | --- | --- |
| Grid churn | 22% every 180 ms | 55% every 50 ms |
| PSI flip | 1100–3800 ms | 320–1150 ms |
| Periscope dwell / slew | 3000 ms / 65°·s⁻¹ | 1150 ms / 175°·s⁻¹ |
| Speech | recipe base | ~50% compressed |

Each supplies its **own** endpoints. Sharing the metric is the point; sharing the actual rate is not — matching every subsystem's timing exactly reads as one clock driving everything, which is the opposite of alive. The periscope is deliberately the least responsive of the three.

Reads must stay **pure** — several modules sample it every pass through `loop()`.

Which interaction a `D3` press fires is **pattern-driven** — agitation bands plus a no-repeat rule, in `interaction.cpp`. See the `r2-sounds` skill.

There is no autonomous idle chirping, by design. The liveliness comes from active mode's movement and light; sound stays manual and deliberate, which is also kinder to everyone standing nearby for an evening.

## Power, as built

`RAW` is fed from a USB power bank with the **`USB+ → RAW` solder jumper closed** on the back of the KB2040. That bypasses the board's 500 mA polyfuse; the servo made it necessary, since amp transients plus a servo stall reach ~1.2 A. The power bank's own protection is now the only current limit, and there is no board-level overcurrent protection at all.

`3V` (the onboard regulator, 500 mA including the MCU) feeds the display and the NeoPixels. Full budget and reasoning in the `hardware` skill.

## Hard rules

Violating any of these breaks the build in a way that is expensive to diagnose:

1. **We use Arduino.** Specifically earlephilhower's `arduino-pico` core, which is what provides `setup1()`/`loop1()`. The core we rejected is Arduino's official *Mbed* core (EOL July 2026). Arduino libraries are expected and fine.
2. **PlatformIO must use maxgerhardt's platform fork.** The official `platform-raspberrypi` ships only the Mbed core. See the `firmware` skill.
3. **`PWMAudio` runs entirely on core1.** Core0 owns display, pixels, and logic. Audio synthesis must never live in `loop()`.
4. **Never set the PWM carrier to ~260 kHz.** It beats against the amplifier's 250 kHz switching at 10 kHz, in the middle of the audio band.
5. **NeoPixels are powered from `3V`, not `RAW`.** This is what makes 3.3 V logic drive them without a level shifter.
6. **The `USB+ → RAW` jumper stays closed.** Reopening it puts the 500 mA polyfuse back in a path that draws up to 1.2 A, and a tripped polyfuse presents as a dead board.
7. **Master volume is the trim pot, not software.** The PWM DAC has 10 bits; attenuating in code throws them away.
8. **No microSD.** Sounds are synthesized, the display draws primitives.

## Where things live

- `README.md` — everything needed with a soldering iron in hand: BOM, board geometry, the mermaid schematic, and the connection tables to wire from.
- `.claude/skills/hardware/` — pin map, power budget, and the electrical reasoning behind the wiring.
- `.claude/skills/firmware/` — PlatformIO, the core, build/flash, concurrency model.
- `.claude/skills/r2-sounds/` — how the voice was actually made, and how to synthesize it here.
- `.claude/skills/bring-up/` — first-power-on order and failure signatures.

Facts live in exactly one of those files. If something needs saying in two places, one of them should link instead of restating.
