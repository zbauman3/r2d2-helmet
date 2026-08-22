---
name: bring-up
description: Ordered first-power-on procedure for this build and a failure-signature lookup table (random resets, silence, hum, dead display, wrong pixel colours) — use at first boot, when a subsystem does not work, or when something that previously worked stops.
---

# Bring-up

## Order, and why

Bring subsystems up **one at a time, in this order**, and do not add the next until the current one is stable. Most of the nasty failures in this build only appear when two subsystems run together, so an ordered bring-up is what makes them attributable.

1. **Bare board.** Flash a blink on the onboard NeoPixel (GP17). Confirms toolchain, upload path, and clock. Nothing else wired.
2. **Display alone.** No speaker, no pixels. This is the highest-risk subsystem — see the ST7789 caveat below. Start SPI at 8–12 MHz.
3. **Audio alone.** Display code still present but not blitting. Confirms the filter, the amp, and the core1 split.
4. **Pixels alone.** Confirms logic level and colour.
5. **All three together.** This is where power problems surface. Have the 470 µF fitted before you get here, not after.
6. **Buttons last.** Trivial, and easier to debug once everything they drive already works. Bring up the mode button (`D2`) before the trigger button (`D3`) — mode is the outer state, and a trigger firing into an unknown mode is harder to read.

## Stage checks

**Stage 1 — bare board.** `pio run -t upload`. If the port does not appear, double-tap the RST pad to GND to force the ROM bootloader; it enumerates as `RPI-RP2`. This always works, so a failure here is never a bricked board.

**Stage 2 — display.** Expect trouble: there are scattered reports of ST7789 problems specifically under arduino-pico ([discussion #1217](https://github.com/earlephilhower/arduino-pico/discussions/1217)). This is unverified for our part and is the first real unknown in the project — settle it before writing any UI code. `tft.init(170, 320)` is the correct init for this panel.

If the backlight is on but the screen is black, the panel has power and the SPI path does not. If the backlight is off entirely, check `Vin` — and remember `Lite` should be *unconnected*, not grounded.

**Stage 3 — audio.** Start the trim pot at minimum and bring it up slowly. Test with a steady mid tone before anything swept — a constant tone makes hum, clipping, and carrier bleed all obvious.

**Stage 4 — pixels.** Test with full white first. It is the worst case for both current and colour, so it shows both problems at once.

**Stage 5 — together.** Run audio and pixels and a full-screen redraw simultaneously, at volume. If it survives a minute of that, the power design is sound.

## Failure signatures

| Symptom | Almost certainly |
| --- | --- |
| Random resets, only once audio *and* lights run together | Brownout. The 470 µF at the amp is missing or too small. Class-D transients sag `RAW`, and the 500 mA polyfuse's series resistance makes it worse |
| Speaker hums, or outputs nothing at all | Floating amp ground. Every ground must tie together — MCU, speaker, pixels, display |
| Audio distorts badly at any volume | Trim pot too high. Gain is a fixed 24 dB and 3.3 V p-p will clip hard |
| Audible whistle around 10 kHz | PWM carrier set near 260 kHz, beating against the amp's 250 kHz switching. Use 130 kHz |
| Audio stutters or clicks on every screen redraw | Synthesis is running on core0. It must be in `setup1()`/`loop1()` |
| Sound is muffled / dull | Expected. The filter is −3 dB at 7.0 kHz. Swap both caps to 4.7 nF if it bothers you |
| Display black, backlight on | SPI path. Check `TCS`/`DC`/`RST` wiring and drop the clock to 8 MHz |
| Display works but microSD does not | `MISO` and `MOSI` swapped — they are pads 5 and 6 in that order, the reverse of most Adafruit boards. (Not applicable: this build has no microSD) |
| Pixels dark or showing garbage | Data logic level. Confirm pixel `V+` goes to `3V`, not `RAW` — at 5 V the threshold is 3.5 V and a 3.3 V GPIO cannot reach it |
| Pixel whites look pink or warm | Expected at 3.3 V — blue and green dies starve first. One silicon diode from `RAW` fixes it |
| First pixel behaves, rest do not | `DOUT`→`DIN` chain break, or the 470 Ω is in the wrong leg |
| Whole thing dies under load, no reset | The 500 mA polyfuse tripped. Everything including the 3.3 V rail runs through it |
| A button fires several times per press | Missing or too-short software debounce. Both buttons are bare switches with no RC |
| Mode toggles on both press *and* release | Edge detection is level-based. Toggle on the HIGH→LOW transition only |
| Both buttons appear pressed at once, or one is stuck LOW | `D2`/`D3` swapped with a `GND` pad — they are pins 5 and 6, immediately below the two grounds at pins 3 and 4 |
| Powers down when idle | Power bank auto-shutoff. Unlikely at ~180 mA idle, but possible with a bank that has a high threshold |

## Do not skip

- **Tie every ground together** before first power-on.
- **Trim pot at minimum** before first power-on.
- **470 µF fitted** before stage 5.

Electrical detail behind all of these is in the `hardware` skill; the core split is in `firmware`.
