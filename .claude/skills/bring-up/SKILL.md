---
name: bring-up
description: Ordered first-power-on procedure for this build and a failure-signature lookup table (random resets, silence, hum, dead display, wrong pixel colours) — use at first boot, when a subsystem does not work, or when something that previously worked stops.
---

# Bring-up

## Order, and why

Bring subsystems up **one at a time, in this order**, and do not add the next until the current one is stable. Most of the nasty failures in this build only appear when two subsystems run together, so an ordered bring-up is what makes them attributable.

1. **Bare board.** Flash a blink on the onboard NeoPixel (GP17). Confirms toolchain, upload path, and clock. Nothing else wired.
2. **Display alone.** No speaker, no pixels. Start SPI at 8–12 MHz. ✅ Working on hardware as of 2026-08-26.
3. **Audio alone.** Display code still present but not blitting. Confirms the filter, the amp, and the core1 split.
4. **Pixels alone.** Confirms logic level and colour.
5. **All three together.** This is where power problems surface. Have the bulk cap fitted before you get here, not after.
6. **Servo**, once the `USB+ → RAW` jumper is closed and its bulk cap is fitted. Bring it up with the speaker unplugged first — if the fuse is going to trip, isolating the two biggest loads tells you which one did it.
7. **Buttons last.** Trivial, and easier to debug once everything they drive already works. Bring up the mode button (`D2`) before the trigger button (`D3`) — mode is the outer state, and a trigger firing into an unknown mode is harder to read.

Stages 2 through 5 do not need the buttons: the firmware accepts **`m` to toggle mode and `t` to fire a trigger** over serial at 115200. That is deliberate, because the wiring order puts the buttons last and the display would otherwise be untestable until the very end.

## Stage checks

**Stage 1 — bare board.** `pio run -t upload`. If the port does not appear, double-tap the RST pad to GND to force the ROM bootloader; it enumerates as `RPI-RP2`. This always works, so a failure here is never a bricked board.

**Stage 2 — display. ✅ Verified 2026-08-26.** The 5394 comes up clean on the wiring as documented: `tft.init(170, 320)` then `setRotation()`, hardware SPI on the variant defaults, 12 MHz. Both buttons and the grid animation confirmed working on real hardware.

This settles the one real unknown the project was carrying. There are scattered reports of ST7789 trouble under arduino-pico ([discussion #1217](https://github.com/earlephilhower/arduino-pico/discussions/1217)) — they do not apply here. Nothing special was needed.

If the backlight is on but the screen is black, the panel has power and the SPI path does not. If the backlight is off entirely, check `Vin` — and remember `Lite` should be *unconnected*, not grounded.

**Stage 3 — audio.** Start the trim pot at minimum and bring it up slowly. Test with a steady mid tone before anything swept — a constant tone makes hum, clipping, and carrier bleed all obvious.

**Stage 4 — pixels.** Test with full white first. It is the worst case for both current and colour, so it shows both problems at once.

**Stage 5 — together.** Run audio and pixels and a full-screen redraw simultaneously, at volume. If it survives a minute of that, the power design is sound.

## Failure signatures

| Symptom | Almost certainly |
| --- | --- |
| Random resets, only once audio *and* lights run together | Brownout. The bulk cap at the amp is missing, too small, or wired backwards. Class-D transients sag `RAW`, and the 500 mA polyfuse's series resistance makes it worse |
| Speaker hums, or outputs nothing at all | Floating amp ground. Every ground must tie together — MCU, speaker, pixels, display |
| Audio distorts badly at any volume | Trim pot too high. Gain is a fixed 24 dB and 3.3 V p-p will clip hard |
| Audible whistle around 10 kHz | PWM carrier set near 260 kHz, beating against the amp's 250 kHz switching. Use 130 kHz |
| Audio stutters or clicks on every screen redraw | Synthesis is running on core0. It must be in `setup1()`/`loop1()` |
| Sound is muffled / dull | Expected. The filter is −3 dB at 7.0 kHz. Swap both caps to 4.7 nF if it bothers you |
| Occasional cells showing a diagonal split for one frame | **Known and accepted — do not chase.** Tearing. The 5394 breaks out no TE pin, so writes cannot be synced to the panel's refresh. Clock speed does not help: the panel takes ~1.5 ms to scan one cell's rows against a 0.3 ms write, so the scan is the wide window. Tested at 12 and 24 MHz, perceptually identical |
| Display black, backlight on | SPI path. Check `TCS`/`DC`/`RST` wiring and drop the clock to 8 MHz |
| Display works but microSD does not | `MISO` and `MOSI` swapped — they are pads 5 and 6 in that order, the reverse of most Adafruit boards. (Not applicable: this build has no microSD) |
| Pixels dark or showing garbage | Data logic level. Confirm pixel `V+` goes to `3V`, not `RAW` — at 5 V the threshold is 3.5 V and a 3.3 V GPIO cannot reach it |
| Pixel whites look pink or warm | Expected at 3.3 V — blue and green dies starve first. One silicon diode from `RAW` fixes it |
| First pixel behaves, rest do not | `DOUT`→`DIN` chain break, or the 470 Ω is in the wrong leg |
| Whole thing dies under load, no reset | The 500 mA polyfuse tripped. Everything including the 3.3 V rail runs through it — with the servo fitted, the `USB+ → RAW` jumper must be closed |
| Everything resets when the periscope moves | Servo inrush. Its 470 µF is missing, too small, or sited at the breakout instead of at the servo |
| Servo buzzes continuously and gets warm | Driving into a mechanical hard stop — a stalled SG92R pulls 650 mA. Narrow `PERISCOPE_ANGLE_MIN`/`MAX` |
| Periscope only sweeps about half its range | `attach()` using the library default 1000–2000 µs instead of the SG92R's 500–2500 |
| A button fires several times per press | Missing or too-short software debounce. Both buttons are bare switches with no RC |
| Mode toggles on both press *and* release | Edge detection is level-based. Toggle on the HIGH→LOW transition only |
| Both buttons appear pressed at once, or one is stuck LOW | `D2`/`D3` swapped with a `GND` pad — they are pins 5 and 6, immediately below the two grounds at pins 3 and 4 |
| Powers down when idle | Power bank auto-shutoff. Unlikely at ~180 mA idle, but possible with a bank that has a high threshold |

## Do not skip

- **Tie every ground together** before first power-on.
- **Trim pot at minimum** before first power-on.
- **Bulk cap fitted** at the amp before stage 5, right way round.

Electrical detail behind all of these is in the `hardware` skill; the core split is in `firmware`.
