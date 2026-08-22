# R2-D2 Costume — BOM and Pin Map

Confirmed parts list, verified pin assignments, and the gotchas specific to this
combination. See `hardware-notes.md` for the toolchain/audio research.

## Bill of materials

| Part | PID | Verified specs |
|---|---|---|
| Adafruit KB2040 | 5302 | RP2040, 133 MHz (per board def), 8 MB flash, **20 GPIO** (18 castellated + 2 on STEMMA QT), 3.3 V reg @ **500 mA** |
| Breadboard NeoPixel, pack of 5 (2 used) | 1312 | **SK6812** (was WS2812S before Aug 2024), 3.5–5 VDC, **~36 mA/pixel** max at full white |
| STEMMA Speaker | 3885 | **PAM8302A** class-D, 1 W into 8 Ω, 3–5 V, on-board 1.0 µF input coupling, volume trim pot |
| 1.9" 320×170 IPS TFT | 5394 | **ST7789**, 4-wire SPI, on-board 3.3 V LDO + 3/5 V level shifter, microSD slot, **1×11 0.1" header** + 18-pin EYESPI FPC connector |

### The TFT breadboards directly — no EYESPI parts needed

Adafruit's guide pinouts page leads with the EYESPI connector, which is what made
this look EYESPI-only at first. It isn't: the product page lists "a 1x11 header for
easy breadboarding," and the board file confirms an 11-pad `JP1` alongside the FPC
connector. Solder the included strip and wire straight into the breadboard. The
EYESPI breakout (5613) and cable (5462/5239) are **not** part of this build.

#### `JP1` pad order, 1 → 11

Taken from the net list in Adafruit's Eagle `.brd` file, then cross-checked against
the breadboard wiring list in the guide's Arduino page. Both agree, including the
`MISO`-before-`MOSI` ordering.

| # | Silk | Net | Notes |
|---|---|---|---|
| 1 | `Vin` | VIN | 3–5 V supply in |
| 2 | `3V` | +3V3 | regulator **output**, ≥100 mA — do not drive |
| 3 | `Gnd` | GND | |
| 4 | `SCK` | SCK | 3–5 V logic |
| 5 | `MISO` | MISO | microSD only — **unwired**, TFT is write-only |
| 6 | `MOSI` | MOSI | |
| 7 | `TCS` | TFTCS | TFT chip select |
| 8 | `RST` | TFTRST | auto-reset circuit onboard |
| 9 | `DC` | TFTDC | data/command select |
| 10 | `SDCS` | CARDCS | microSD chip select — **unwired** |
| 11 | `Lite` | LITE | backlight enable, **pulled high by default — left unconnected** |

Silkscreen is printed in full on the top layer (`Vin`, `3V`, `Gnd`, …); the guide
also documents two-letter abbreviations (`V+`, `3V`, `G`, `CK`, `SO`, `SI`, `TC`,
`RT`, `DC`, `CC`, `BL`) for the same pads.

Because `Lite` idles high, the backlight runs full on with the pad unconnected.
This build keeps it that way — no dimming, no GPIO spent, one less wire. `A2` /
GP28 is free as a result. Driving `Lite` later would mean fighting the onboard
pull-up, not a floating node.

Sources: [product page](https://www.adafruit.com/product/5394) ·
[guide pinouts](https://learn.adafruit.com/adafruit-1-9-color-ips-tft-display/pinouts) ·
[Arduino wiring page](https://learn.adafruit.com/adafruit-1-9-color-ips-tft-display/arduino-wiring-test) ·
[PCB repo, `JP1` net list](https://github.com/adafruit/Adafruit-1.9in-320x170-Color-IPS-TFT-PCB)

### NeoPixel logic level — settled: power from `3V`

SK6812 wants a data high of 0.7 × VDD. At a 5 V supply that's 3.5 V and the RP2040
only puts out 3.3 V, which is why Adafruit's Uberguide says driving 5 V NeoPixels
from a 3.3 V micro means "you must use a logic level shifter."

**Decision: power the pixels from the KB2040's `3V` pin, not `RAW`.** At VDD =
3.3 V the required data high drops to 2.31 V and the 3.3 V GPIO has plenty of
margin — no 74AHCT125. Colors run slightly dim, which is irrelevant behind a
diffuser in a dim room.

Note this is a hair under spec: Adafruit rates the part 3.5–5 V. It works because
SK6812's threshold scales with its own supply. If a future build needs full
brightness, the in-spec version without a shifter is `RAW` through one silicon
diode (1N4001) for ~4.3 V, where VIH is 3.0 V.

Either way: **470 Ω in series on the data line** (Uberguide says 300–500 Ω), and
**100 µF across the pixels' power pins**.

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
| NeoPixel data | D5 | GP5 | 2B | PIO state machine; 470 Ω series |
| Periscope servo | D6 | GP6 | 3A | arduino-pico `Servo` is PIO-based |
| TFT SCK | SCK | GP18 | — | SPI0 |
| TFT MOSI | MOSI | GP19 | — | SPI0 |
| TFT CS | D10 | GP10 | — | digital |
| TFT D/C | D7 | GP7 | — | digital |
| TFT RST | D4 | GP4 | — | digital |
| Trigger button | D2 | GP2 | 1A | `INPUT_PULLUP`, switch to GND, debounced in software |

No microSD in this build — sounds are synthesized and the display draws
primitives, so GP9 and GP20 stay free.

Free for expansion: GP0/GP1 (Serial1), GP3, GP9, GP20, GP12/GP13 (STEMMA QT),
GP26/GP27, GP28, GP29.

RP2040 maps GPIO to PWM slices as `(gpio >> 1) & 7`, even GPIO → channel A, odd →
channel B. Overlaps in the map above:

| Slice | Pins | Matters? |
|---|---|---|
| 4 | GP8 (A), GP9 (B) | **Yes** — `PWMAudio` owns slice 4, so GP9 gets no `analogWrite()` if it's ever used |
| 2 | GP4 (A), GP5 (B) | No — GP4 is digital, GP5 is PIO |
| 3 | GP6 (A), GP7 (B) | No — `Servo` is PIO-based, GP7 is digital |
| 1 | GP18 (A), GP19 (B), GP2 (**also A**) | No — SPI0 plus a digital input |

GP2 and GP18 land on the same slice *and* the same channel, as would GP4 and GP20
if the microSD were ever wired. None of them need PWM.

## Power

Source: a **USB power bank** into the KB2040's USB-C.

| Rail | Feeds | Draw |
|---|---|---|
| `RAW` (5 V, **500 mA polyfuse** + polarity diode) | STEMMA Speaker VIN, *and the 3.3 V regulator* | ~350 mA peaks at 1 W into 8 Ω |
| `3V` (500 mA total, incl. MCU) | NeoPixels, TFT `Vin` | 2 × 36 mA = 72 mA worst case + ~80 mA display + ~30 mA MCU ≈ 180 mA |

Everything runs through that 500 mA polyfuse, including the 3.3 V rail: ~280 mA
average, ~530 mA on transients. It holds, but two consequences follow. The fuse's
series resistance worsens transient sag, which is another reason the bulk cap is
mandatory. And a periscope servo (0.5–1 A stall) would exceed it outright — the
KB2040 has a `USB+ → RAW` solder jumper on the back that bypasses the fuse for up
to 2 A if that day comes.

Most power banks cut out below ~50–100 mA of draw. Idle here is ~180 mA, well
clear, so no keepalive load is needed.

**Add bulk capacitance at the amp** — 470 µF across the speaker board's power
pins. Class-D current spikes on transients will sag `RAW` and can brown out the
MCU. This is the most likely cause of mystery resets once audio and lights run
together. A second 100 µF sits across the NeoPixel supply.

The 3.3 V rail has ~320 mA of headroom at 2 pixels, so the pixel count can grow
later without revisiting this.

## Audio output network

```
GP8 ──[ 1k ]──┬──[ 1k ]──┬──> STEMMA Speaker signal in
              │          │
            [10n]      [10n]
              │          │
             GND        GND
```

**15.9 kHz is the corner of one *isolated* section — not of this network.** The two
sections are unbuffered, so they load each other (denominator `1 + 3sRC + (sRC)²`,
not `(1 + sRC)²`), and the speaker's 10 kΩ trim pot hangs off the output as a third
load. Measured response, in dB, including that loading:

| Network | 3 k | 5 k | 8 k | 11 k | 130 k | −3 dB |
|---|---|---|---|---|---|---|
| **2× (1 kΩ, 10 nF)** — as wired | −2.3 | −3.3 | −5.2 | −7.1 | **−36.9** | **7.0 kHz** |
| 2× (1 kΩ, 6.8 nF) | −1.9 | −2.5 | −3.6 | −4.9 | −30.7 | 10.3 kHz |
| 2× (1 kΩ, 4.7 nF) | −1.8 | −2.0 | −2.7 | −3.5 | −25.1 | 14.9 kHz |

As wired this trades a soft top end for −37 dB of carrier rejection, which is the
right way round for a first build — the small enclosed 8 Ω speaker rolls off up
there anyway. Swap both caps to 4.7 nF if it sounds muffled.

**Never set the carrier to 260 kHz.** The PAM8302A switches at ~250 kHz; a 260 kHz
carrier beats against it at 10 kHz, in the middle of the audio band. That rules out
9-bit at 133 MHz. 130 kHz (10-bit, used here) and 520 kHz (8-bit) are both clear.

No DC blocking cap and no divider is needed from us — see the amp's own input
network below.

### What the STEMMA Speaker actually does

Read off `Adafruit STEMMA Speaker.sch`, because the pinouts page documents none of
it:

- The **10 kΩ trim pot is a divider straight across `SIG` → `GND`**; the wiper
  feeds a 1 µF cap, then a 100 Ω series resistor, then `IN_P`. So `SIG` carries a
  permanent 10 kΩ load, and the coupling cap sits *after* the pot, not at the pin.
- **Gain is a fixed 24 dB.** `R12`/`R13` (100 Ω) are EMI series resistors, not
  gain-setting — this is the PAM8302**A**, the fixed-gain variant. The pot is the
  only hardware volume control.
- **`/SD` is tied to `VDD`.** No software mute; the amp is live whenever `RAW` is.

Practical consequence: **drive the PWM near full scale and set loudness with the
pot.** The amp's noise floor is downstream of the pot and doesn't move, while every
6 dB of software attenuation costs a bit of a 10-bit DAC. Reserve software gain for
expression within a sound.

Set the pot **low** to start — 3.3 V p-p into a fixed 24 dB of gain will clip hard.

Convenient side effect: our 2 kΩ of series R against the pot's 10 kΩ divides the
3.3 V swing to 2.75 V at `SIG`, landing inside Adafruit's stated 0–3 V input range
with no extra divider.

Schematic source:
[Adafruit-STEMMA-Speaker-PCB](https://github.com/adafruit/Adafruit-STEMMA-Speaker-PCB)

## Software init

```cpp
// Carrier must be set explicitly — PWMAudio defaults to a 48 kHz carrier,
// barely above the audio band. 130 kHz keeps clear of the amp's ~250 kHz
// switching; 260 kHz would beat against it at 10 kHz.
audio.begin(/*sampleRate*/ 22050, /*carrierHz*/ 130000);  // 133 MHz / 1024 => ~10-bit
```

This runs on **core1** — `setup1()` / `loop1()` — not core0. A full-screen blit is
~109 KB, about 36 ms of blocking SPI, against PWMAudio's default ~23 ms of
buffering, so synthesis cannot share a core with the display. Calling `begin()`
from core1 also puts the DMA IRQ on core1's NVIC, where the `noInterrupts()` inside
`NeoPixel::show()` on core0 cannot reach it.

`pins_arduino.h` defines `PIN_SPI0_SCK/MOSI/MISO` as GP18/19/20 — exactly the
wiring above — so the default `SPI` object works with no `setSCK()` calls. Note
`SPI_HOWMANY` is 1: this board pins out only one SPI bus.

Start the display at 8–12 MHz and raise it once it's stable; 24 MHz over breadboard
jumpers is optimistic.
