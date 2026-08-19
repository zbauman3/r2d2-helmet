# R2-D2 Costume — Verified Hardware Notes

Fact-check pass over the original handoff doc, August 2026. Every claim below was
checked against a primary source (datasheet, library source, or vendor page).
Sources are listed at the bottom.

---

## Corrections to the handoff doc

### 1. Do not hand-roll the PWM audio ISR — `PWMAudio` already exists

The doc plans a timer ISR that writes duty values. Philhower's core ships a
`PWMAudio` library that does this better:

- Samples are fed as **signed 16-bit**, auto-scaled down to whatever the PWM
  hardware resolution allows.
- The sample rate is paced by a **DMA timer** (`dma_claim_unused_timer`), not by a
  CPU interrupt. There is no per-sample ISR at all.
- Refills happen per buffer via an `onTransmit()` callback.
- Mono works on any pin. Stereo requires an even pin (odd pin becomes the right
  channel). No `analogWrite()` to any other pin on the same PWM slice.

This matters beyond convenience — see item 3.

**Carrier frequency default is a trap.** `PWMAudio`'s constructor sets
`_freq = 48000`, i.e. a **48 kHz carrier**, which is barely above the audio band.
Set it explicitly:

```cpp
audio.begin(/*sampleRate*/ 22050, /*pwmCarrierHz*/ 130000);
```

`setPWMFrequency()` derives the wrap value as `sys_clk / freq`, so the doc's
resolution/carrier table is correct in principle — but at the **133 MHz** the
KB2040 board definition actually uses, not 125 MHz:

| Wrap (resolution) | Carrier @ 133 MHz |
|---|---|
| 256 (8-bit) | 519.5 kHz |
| 512 (9-bit) | 259.8 kHz |
| 1024 (10-bit) | 129.9 kHz |

(`f_cpu: "133000000L"` is set in `boards/adafruit_kb2040.json`. Note that above
125 MHz the SDK switches `peri_clk` feeding SPI/UART to 48 MHz.)

### 2. The NeoPixel problem is real, but the doc has the mechanism wrong

The doc says NeoPixel writes are bit-banged. On RP2040 they are **not** — the
library uses a PIO state machine (`pio_claim_free_sm_and_add_program_for_gpio_range`,
1 state machine of the 8 available).

But the interrupt problem still exists, for a worse reason. In `Adafruit_NeoPixel.cpp`
the guard around `show()` is:

```c
#if !(defined(NRF52) || defined(NRF52_SERIES) || defined(ESP32))
  noInterrupts(); // Need 100% focus on instruction timing
#endif
```

RP2040 is **not** in that exclusion list, so interrupts are still globally masked —
and `rp2040Show()` feeds the FIFO with `pio_sm_put_blocking()` one byte at a time:

```c
while (numBytes--)
  pio_sm_put_blocking(pio, pio_sm, ((uint32_t)*pixels++) << 24);
```

That blocks for roughly `numBytes × 10 µs`. Thirty RGB pixels ≈ **900 µs with
interrupts off**. This is a known open bug (Adafruit_NeoPixel issue #441, opened
July 2025, no maintainer response).

**Consequence:** the doc's mitigation #2 ("drive the NeoPixels from PIO") does not
help — they already are. What actually fixes it:

- **Use `PWMAudio`.** DMA does not care about `PRIMASK`. Audio keeps streaming
  through the blackout; only the buffer-refill callback is delayed, which adequate
  buffering absorbs. This alone resolves it.
- **Split across cores** (doc's mitigation #1) still works — `noInterrupts()` is
  `__disable_irq()`, and PRIMASK is per-core on Cortex-M0+.
- `Adafruit_NeoPXL8` uses PIO **plus DMA** and does not block.

### 3. The amp is a PAM8302A, and the "no extra parts needed" claims hold

STEMMA Speaker PID 3885: **PAM8302A** class-D, 1 W into the onboard 8 Ω speaker,
3–5 V supply, screwdriver volume trim.

Both of the doc's "not required" claims are confirmed by Adafruit directly:

- Input is AC-coupled on-board through **1.0 µF** capacitors — no DC blocking cap.
- Input "can range up to the power pin voltage (3 or 5 V peak-to-peak)" — a 3.3 V
  PWM swing is in spec, no divider.

**Practical caveat the doc misses:** the PAM8302A has gain. Driving it with a full
3.3 V p-p signal means running the trim pot near minimum to avoid clipping into a
1 W speaker. Budget some attenuation into the RC network rather than relying on the
pot alone.

The **RC filter requirement stands.** `1 kΩ + 10 nF` → corner at 15.9 kHz (the doc's
"~16 kHz" is right). It matters more than the doc implies: the PAM8302A's own
class-D output switches at ~250 kHz, so an unfiltered carrier at the input produces
intermodulation products that land *in* the audio band. Cascading two sections is
worth it.

### 4. Mbed EOL — the date is July 2026, and it has passed

The doc says "announced in 2024," which is right, but the actual end-of-life date is
**July 2026** — i.e. last month. Mbed OS remains open source but is unmaintained by
Arm. Arduino's replacement path is Zephyr, not a fix for the Mbed RP2040 core.

This strengthens the doc's recommendation rather than changing it: Philhower's core
is the correct choice.

### 5. PlatformIO: the official platform still does not support Philhower

The doc's toolchain section doesn't say how to pin this, and it's easy to get wrong.
Verified against `platform.json` on `platformio/platform-raspberrypi@develop` (v1.20.0):
the frameworks block lists **only** `framework-arduino-mbed`. Community claims that
mainline "now supports" the Philhower core are wrong. The arduino-pico docs still
direct users to `maxgerhardt/platform-raspberrypi`.

Working config is in `platformio.ini`. The board id is **`adafruit_kb2040`**, not
`kb2040` — and `board_build.core` is unnecessary because that board JSON hardcodes
`"core": "earlephilhower"`.

The same board id also declares `"frameworks": ["arduino", "picosdk"]`, so the
"should we drop to the bare SDK?" question does not require a project rewrite —
it's a one-line change. (The doc's warning that Arduino libraries won't survive
that switch is correct.)

### 6. R2's voice was not purely synthetic

The doc says the voice "originated as swept tones from an ARP 2600." Incomplete in a
way that affects the synthesis plan: Ben Burtt blended **his own voice** with the
ARP patch, feeding a microphone through the synth's self-resonating filter and
performing lines live while twiddling the filter. The sample-and-hold LFO through
the resonant filter produced the chirps; the human input produced the *phrasing*.

The doc's three synthesis rules (exponential sweeps, 2–3 random anchor pitches,
overshoot before settling) are all sound and are effectively an attempt to recover
that phrasing algorithmically. Worth keeping, and worth knowing that a purely
synthetic result will always be missing a vocal/formant layer.

---

## Board budget — KB2040

Worth checking before committing to the feature list. The KB2040 has **20 GPIO**
(18 castellated + 2 on the STEMMA QT port) and a **500 mA** 3.3 V regulator.

Contention to watch:
- **PIO state machines (8 total):** NeoPixel takes 1. The arduino-pico `Servo`
  library is also PIO-based. Both fit, but a PIO-driven display would not be free.
- **PWM slices (8, two outputs each):** `PWMAudio` owns a whole slice. No
  `analogWrite()` to the sibling pin.
- **DMA:** `PWMAudio` claims channels *and* a DMA timer.
- **RAW pin** is the 5 V feed for NeoPixels; fused at 500 mA–1 A by default.

## Still open

- **Logic display panel.** Not resolved. Both `ST7789` (1.54" 240×240 square) and
  `GC9107` (0.85" 128×128) are supported by `Arduino_GFX`; the 8×8 I2C LED backpack
  is Adafruit_GFX-compatible and much closer to the screen-used look. Note that the
  astromech builder community (Teeces, RSeries LogicEngine) uses **LED matrices**,
  not TFTs, for exactly this part — worth looking at before buying a screen.
  There are scattered reports of ST7789 trouble specifically under arduino-pico
  (arduino-pico discussion #1217) — verify before committing.
- Restraining bolt hall-sensor gag.
- Cosmetic prod.

## Sources

- [Adafruit STEMMA Speaker (PID 3885)](https://www.adafruit.com/product/3885) ·
  [downloads / PAM8302A](https://learn.adafruit.com/adafruit-stemma-speaker/downloads)
- [arduino-pico PWMAudio docs](https://arduino-pico.readthedocs.io/en/latest/pwm.html) ·
  [PWMAudio.cpp](https://github.com/earlephilhower/arduino-pico/blob/master/libraries/PWMAudio/src/PWMAudio.cpp)
- [arduino-pico PlatformIO docs](https://arduino-pico.readthedocs.io/en/latest/platformio.html) ·
  [platform-raspberrypi platform.json](https://github.com/platformio/platform-raspberrypi)
- [Adafruit_NeoPixel issue #441](https://github.com/adafruit/Adafruit_NeoPixel/issues/441) ·
  [Adafruit_Neopixel_RP2.cpp](https://github.com/adafruit/Adafruit_NeoPixel/blob/master/Adafruit_Neopixel_RP2.cpp)
- [Adafruit_NeoPXL8](https://github.com/adafruit/Adafruit_NeoPXL8)
- [The end of Mbed marks a new beginning for Arduino](https://blog.arduino.cc/2024/07/24/the-end-of-mbed-marks-a-new-beginning-for-arduino/) ·
  [Mbed OS is end of life July 2026](https://blog.adafruit.com/2026/02/02/a-reminder-that-mbed-os-is-end-of-life-july-2026/)
- [Adafruit KB2040 pinouts](https://learn.adafruit.com/adafruit-kb2040/pinouts)
- [How the ARP 2600 created R2-D2](https://www.redbull.com/ca-en/playing-with-arp-2600-star-wars-r2d2) ·
  [Ben Burtt Special: Star Wars – The Sounds](https://designingsound.org/2009/09/10/ben-burtt-special-star-wars-the-sounds-part-2/)
