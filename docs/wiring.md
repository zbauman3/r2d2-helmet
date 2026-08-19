# R2-D2 Costume — BOM and Pin Map

Confirmed parts list, verified pin assignments, and the gotchas specific to this
combination. See `hardware-notes.md` for the toolchain/audio research.

## Bill of materials

| Part | PID | Verified specs |
|---|---|---|
| Adafruit KB2040 | 5302 | RP2040, 133 MHz (per board def), 8 MB flash, **20 GPIO** (18 castellated + 2 on STEMMA QT), 3.3 V reg @ **500 mA** |
| Breadboard NeoPixel, pack of 5 | 1312 | **SK6812** (was WS2812S before Aug 2024), 3.5–5 VDC, **~36 mA/pixel** max at full white |
| STEMMA Speaker | 3885 | **PAM8302A** class-D, 1 W into 8 Ω, 3–5 V, on-board 1.0 µF input coupling, volume trim pot |
| 1.9" 320×170 IPS TFT | 5394 | **ST7789**, 4-wire SPI, on-board 3.3 V LDO + 3/5 V level shifter, microSD slot, **18-pin EYESPI FPC connector** |

### Two things to check before ordering is done

**1. The TFT is probably EYESPI-only.** Every Adafruit source for 5394 — product
page, guide overview, guide pinouts — documents exactly one connector: the 18-pin
0.5 mm FPC. No 0.1" header row appears in any of them. Look at the physical board
for solder pads along the edge; if there aren't any, you also need:

- **EYESPI Breakout Board** (PID 5613) — breaks the 18 pins out to breadboard pitch
- **EYESPI cable** (PID 5462, 50 mm, or 5239, 100 mm)

**2. NeoPixel logic level.** SK6812 at a 5 V supply wants a data high of 0.7 × VDD
= 3.5 V. The RP2040 outputs 3.3 V. Adafruit's own Uberguide says outright: driving
5 V NeoPixels from a 3.3 V micro means "you must use a logic level shifter."

For 5 pixels there is a simpler fix than a 74AHCT125 — **power them from the
KB2040's `3V` pin instead of `RAW`**. At VDD = 3.3 V the required data high drops
to 2.31 V and the 3.3 V GPIO is comfortably in spec. Colors run slightly dim, which
is irrelevant behind a diffuser. Adafruit rates the part 3.5–5 V so this is a hair
under spec; if you want it fully in spec without a shifter, feed them from `RAW`
through one silicon diode (1N4001) for ~4.3 V, where VIH is 3.0 V.

Either way: **300–500 Ω in series on the data line**, per the Uberguide.

## Aspect ratio is a feature, not a problem

The original plan called for a "small square screen." 5394 is 320×170 — roughly
1.88:1, with a **46 mm × 25 mm** visible area (~1.8" × 1.0").

That is closer to correct, not further from it. R2's front logic displays are wide
rectangles, not squares. One 5394 can render a single FLD at slightly over scale,
or both stacked FLDs split across the one panel.

**Mechanical warning from Adafruit, and it matters for a costume:** this panel was
designed for smartwatches, where cover glass holds it down. "Without something
gently holding the screen down, the backlight can eventually peel away from the
TFT." Design the dome faceplate/bezel to press lightly on the display face.

## Pin map

KB2040 GPIO assignments verified against `variants/adafruit_kb2040/pins_arduino.h`
in arduino-pico (SPI0 = GP18/19/20, Wire0 = GP12/13 on STEMMA QT, Wire1 = GP2/3 on
breakout pins, onboard NeoPixel = GP17).

| Function | Silkscreen | GPIO | PWM slice | Notes |
|---|---|---|---|---|
| Audio PWM → RC → amp | D8 | GP8 | 4A | `PWMAudio` claims the whole slice |
| NeoPixel data | D5 | GP5 | 2B | PIO state machine; 300–500 Ω series |
| Periscope servo | D6 | GP6 | 3A | arduino-pico `Servo` is PIO-based |
| TFT SCK | SCK | GP18 | — | SPI0 |
| TFT MOSI | MOSI | GP19 | — | SPI0 |
| TFT MISO | MISO | GP20 | — | only needed for microSD |
| TFT CS | D10 | GP10 | — | digital |
| TFT D/C | D7 | GP7 | — | digital |
| TFT RST | D4 | GP4 | — | digital |
| TFT backlight | A2 | GP28 | 6A | PWM dimming, no slice conflict |
| SD CS (optional) | D9 | GP9 | 4B | digital only — slice 4 belongs to audio |

Free for the restraining-bolt hall sensor and expansion: GP0/GP1 (Serial1),
GP2/GP3 (Wire1), GP12/GP13 (STEMMA QT), GP26/GP27, GP29.

RP2040 maps GPIO to PWM slices as `(gpio >> 1) & 7`, so the pairs sharing a slice
here are GP8/GP9, GP18/GP19, GP4/GP5, GP26/GP10. Only GP8/GP9 matters, and GP9 is
digital-only above.

## Power

| Rail | Feeds | Draw |
|---|---|---|
| `RAW` (5 V, fused 500 mA–1 A) | STEMMA Speaker VIN | ~350 mA peaks at 1 W into 8 Ω |
| `3V` (500 mA total, incl. MCU) | NeoPixels, TFT VIN | 5 × 36 mA = 180 mA worst case + ~80 mA display + ~30 mA MCU ≈ 290 mA |

**Add bulk capacitance at the amp** — 100–470 µF across the speaker board's power
pins. Class-D current spikes on transients will sag `RAW` and can brown out the
MCU. This is the most likely cause of mystery resets once audio and lights run
together.

## Audio output network

```
GP8 ──[ 1k ]──┬──[ 1k ]──┬──> STEMMA Speaker signal in
              │          │
            [10n]      [10n]
              │          │
             GND        GND
```

One section gives a 15.9 kHz corner; the second gets the carrier down far enough
that it doesn't intermodulate with the PAM8302A's own ~250 kHz switching. No DC
blocking cap and no divider — the speaker board AC-couples through 1.0 µF and
accepts up to its supply rail peak-to-peak.

Set the trim pot **low** to start. The PAM8302A has gain, and 3.3 V p-p into a 1 W
speaker will clip hard with the pot up.

## Software init

```cpp
// Carrier must be set explicitly — PWMAudio defaults to a 48 kHz carrier,
// barely above the audio band.
audio.begin(/*sampleRate*/ 22050, /*carrierHz*/ 130000);  // 133 MHz / 1024 => ~10-bit
```
