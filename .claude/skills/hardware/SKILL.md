---
name: hardware
description: KB2040 pin assignments, PWM slice map, power budget, and the electrical gotchas specific to this parts combination (SK6812 pixels, PAM8302A amp, ST7789 display) — use when wiring, changing or adding a pin assignment, budgeting current, choosing component values, or debugging anything electrical.
---

# Hardware reference

This file is the _reasoning_. The wiring tables you actually connect from — BOM, board geometry, the mermaid schematic, per-net connection tables, and the 5394's 1×11 header pad order — are in `README.md`. Do not duplicate them here.

## Parts

| Part                   | PID  | Notes                                                     |
| ---------------------- | ---- | --------------------------------------------------------- |
| Adafruit KB2040        | 5302 | RP2040 @ 133 MHz, 8 MB flash, 20 GPIO, 3.3 V reg @ 500 mA |
| Breadboard NeoPixel ×2 | 1312 | SK6812, 3.5–5 VDC, ~36 mA/pixel at full white             |
| STEMMA Speaker         | 3885 | PAM8302A class-D, 1 W into 8 Ω, fixed 24 dB gain          |
| 1.9" 320×170 IPS TFT   | 5394 | ST7789, onboard 3.3 V LDO + 3/5 V level shifter           |
| TowerPro SG92R         | 169  | 9 g servo, 4.8 V nom (3–6 V), 2.5 kg·cm, 0.1 s/60°, **650 mA stall**, 500–2500 µs |

Powered from a USB power bank into USB-C.

## Pin map

Verified against `variants/adafruit_kb2040/pins_arduino.h` (arduino-pico) and `ports/raspberrypi/boards/adafruit_kb2040/pins.c` (CircuitPython), which agree. **Do not trust Adafruit's KB2040 pinouts guide page** — its prose produced a wrong map on the first pass (it appears to claim `D10` = GPIO 11 and STEMMA QT = GP4/GP5, which contradicts `D4`/`D5` already being GP4/GP5).

| Silkscreen | GPIO | PWM slice | Purpose                               |
| ---------- | ---- | --------- | ------------------------------------- |
| D8         | GP8  | 4A        | Audio PWM out → RC filter             |
| D5         | GP5  | 2B        | NeoPixel data                         |
| D2         | GP2  | 1A        | Mode button → GND, `INPUT_PULLUP`     |
| D3         | GP3  | 1B        | Trigger button → GND, `INPUT_PULLUP`  |
| D7         | GP7  | 3B        | TFT D/C                               |
| D4         | GP4  | 2A        | TFT reset                             |
| D10        | GP10 | 5A        | TFT chip select                       |
| SCK        | GP18 | 1A        | TFT clock (SPI0)                      |
| MOSI       | GP19 | 1B        | TFT data (SPI0)                       |
| D6         | GP6  | 3A        | Periscope servo (PIO-driven, not PWM) |
| A2         | GP28 | 6A        | TFT backlight, PWM                    |

Free: GP0/GP1 (Serial1), GP9, GP20, GP12/GP13 (STEMMA QT), GP26/GP27, GP29. GP11 is the onboard BOOT button and GP17 the onboard NeoPixel; neither is castellated.

Both buttons sit on GP2/GP3, which is `Wire1` — so **`Wire1` is no longer available**. I2C, if it is ever needed, goes on the STEMMA QT port (`Wire0`, GP12/GP13), which is still free and needs no soldering.

GP2 and GP3 are pins 5 and 6 on the board, immediately below the two `GND` pads at pins 3 and 4. That is why the buttons live there — each is a two-pad hop on the breadboard rather than a wire across it.

### PWM slice overlaps

RP2040 maps GPIO to slices as `(gpio >> 1) & 7`; even GPIO → channel A, odd → B.

| Slice | Pins in use                          | Matters?                                                                                 |
| ----- | ------------------------------------ | ---------------------------------------------------------------------------------------- |
| 4     | GP8 (A), GP9 (B)                     | **Yes.** `PWMAudio` owns the whole slice — GP9 gets no `analogWrite()` if it's ever used |
| 1     | GP18 (A), GP19 (B), GP2 (**also A**), GP3 (**also B**) | No — SPI0 plus two digital inputs                                      |
| 2     | GP4 (A), GP5 (B)                     | No — GP4 digital, GP5 is PIO                                                             |
| 3     | GP6 (A), GP7 (B)                     | No — `Servo` is PIO-based, GP7 digital                                                   |

Slice 1 is fully occupied: GP2/GP18 share channel A and GP3/GP19 share channel B. Harmless, since none of the four needs PWM — but nothing else on slice 1 can use `analogWrite()`.

## Power

| Rail                                              | Feeds                                    | Draw                                                |
| ------------------------------------------------- | ---------------------------------------- | --------------------------------------------------- |
| `RAW` — 5 V, **polyfuse bypassed** (see below) | speaker amp, periscope servo, **and the 3.3 V regulator** | ~350 mA amp transient + ~150 mA servo moving        |
| `3V` — 500 mA _including the MCU_                 | NeoPixels, TFT `Vin`                     | 72 mA pixels + ~80 mA display + ~30 mA MCU ≈ 180 mA |

**Everything runs through `RAW`**, the 3.3 V rail included. As shipped that path goes via a 500 mA polyfuse; **on this build the `USB+ → RAW` jumper is closed and the fuse is out of circuit** — see below for why.

Bulk capacitance at the amp is **mandatory** regardless — class-D current spikes otherwise brown out the MCU. It shows up as random resets that only appear once audio and lights run together. Bypassing the fuse helped by removing its series resistance from the path, but it did not remove the need for local charge storage.

Built with **1000 µF**. The value is not tuned; anything from 220 µF up does the job, and more is better as long as it fits. Several smaller caps in parallel are marginally better than one large one, since ESR parallels along with capacitance.

### The fuse bypass — done, and why

**Status: the `USB+ → RAW` solder jumper on the back of the KB2040 is closed.** The 500 mA polyfuse is bypassed and `RAW` is limited only by USB and the power bank, up to ~2 A.

The servo forced it. With the SG92R fitted the stock budget did not close:

| Load | Draw |
| --- | --- |
| 3.3 V rail via the LDO | ~180 mA |
| Speaker amp | ~350 mA transient |
| SG92R | ~150 mA moving, **650 mA stalled** |
| Worst case | **~1180 mA** against the stock 500 mA fuse |

Two things follow from the bypass:

- **`RAW` sits closer to a true 5 V under load**, since the fuse's series resistance is gone. The SG92R is 4.8 V nominal and was previously running slightly under, so this helps it and the amp both.
- **There is no board-level overcurrent protection any more.** The power bank is the only limit. A short on `RAW` will not be caught by anything on the board.

If the jumper is ever reopened, the servo must come out with it — a tripped polyfuse looks exactly like a dead board until it cools, which is a miserable failure to diagnose.

Fit **bulk capacitance across the servo's supply at the servo** (1000 µF as built), not back at the breakout. Inrush is what browns out everything else, and local capacitance supplies it instead of pulling it down a shared wire. Run the servo's ground back to the common point short and thick for the same reason.

The alternative, if the jumper is unappealing: feed the servo directly from the power bank through a USB breakout and share only ground with the KB2040. Electrically cleaner — the servo's spikes never touch the MCU's rail — at the cost of one more part and a second cable.

A stall is the case that matters: 650 mA drawn continuously until something moves or gives. Keep `PERISCOPE_ANGLE_MIN`/`MAX` inside what the hinge mechanically allows so the horn never drives into a hard stop.

Idle draw ~180 mA is well clear of the ~50–100 mA cutoff most power banks use, so no keepalive load is needed.

## NeoPixels: why 3.3 V

SK6812 needs a data high of `0.7 × VDD`. At a 5 V supply that's 3.5 V and the RP2040 only puts out 3.3 V — marginal, and Adafruit's Uberguide says to use a level shifter. Powering the pixels at 3.3 V instead drops the threshold to 2.31 V, which a 3.3 V GPIO drives with plenty of margin. No 74AHCT125 needed.

The cost: 3.3 V is under SK6812's rated 3.5 V minimum. Blue and green dies have the highest forward voltage and starve first, so **whites skew warm**. Dimness was accepted deliberately; the colour shift is the part that might not be.

If the colour matters, the fix is one silicon diode from `RAW` → pixel `V+`, giving ~4.0–4.3 V where V_IH is 2.8–3.0 V and 3.3 V logic still drives it. That also moves the pixel current off the 3.3 V regulator.

470 Ω in series on the data line (Uberguide says 300–500 Ω) and 100 µF across the pixel supply.

## Display

The 5394 has both an 18-pin EYESPI FPC connector _and_ a 1×11 0.1" breadboard header. Adafruit's guide pinouts page leads with EYESPI and never shows the header, which is misleading — the product page says "a 1x11 header for easy breadboarding" and the Eagle board file confirms an 11-pad `JP1`. **No EYESPI breakout or cable is part of this build.**

Three traps, all in the pad order (which is in `README.md`):

- **`MISO` sits before `MOSI`** (pads 5 and 6), the reverse of most Adafruit breakouts. Swapping them still gives a working display, because the TFT is write-only — you'd only find out when microSD failed to mount.
- **Pad 2 `3V` is a regulator output, not an input.** Power goes to `Vin` (pad 1). Driving pad 2 ties two regulator outputs together.
- **`Lite` is pulled high on-board**, so an unconnected pad means a permanently lit backlight. It is driven from `A2`/GP28 so standby can actually go dark. Driving it works against that pull-up rather than a floating node — a few hundred microamps, nothing a GPIO notices, and slice 6 has nothing else on it. Leave it unwired and the panel simply stays lit, so this is an enhancement rather than a dependency.

Mechanical, and it matters for a costume: this panel was designed for smartwatches where cover glass retains it. Adafruit warns that "without something gently holding the screen down, the backlight can eventually peel away from the TFT." The dome faceplate must press lightly on the display face.

Aspect ratio is a feature — 320×170 is ~1.88:1 with a 46 × 25 mm visible area, and R2's front logic displays are wide rectangles, not squares.

## Audio output network

```
GP8 ──[ 1k ]──┬──[ 1k ]──┬──> STEMMA Speaker SIG
              │          │
            [10n]      [10n]
              │          │
             GND        GND
```

**15.9 kHz is the corner of one isolated section, not of this network.** The two sections are unbuffered so they load each other — denominator `1 + 3sRC + (sRC)²`, not `(1 + sRC)²` — and the amp's 10 kΩ trim pot hangs off the output as a third load. Response in dB, including that loading:

| Network                         | 3 k  | 5 k  | 8 k  | 11 k | 130 k     | −3 dB       |
| ------------------------------- | ---- | ---- | ---- | ---- | --------- | ----------- |
| **2× (1 kΩ, 10 nF)** — as wired | −2.3 | −3.3 | −5.2 | −7.1 | **−36.9** | **7.0 kHz** |
| 2× (1 kΩ, 6.8 nF)               | −1.9 | −2.5 | −3.6 | −4.9 | −30.7     | 10.3 kHz    |
| 2× (1 kΩ, 4.7 nF)               | −1.8 | −2.0 | −2.7 | −3.5 | −25.1     | 14.9 kHz    |

As wired this trades a soft top end for −37 dB of carrier rejection, which is the right way round for a first build — the small enclosed 8 Ω speaker rolls off up there anyway. If it sounds muffled, swapping both caps to 4.7 nF is a two-part change. Synthesis should not put much energy above ~7 kHz and expect to hear it.

## What the STEMMA Speaker actually does

Read off `Adafruit STEMMA Speaker.sch`, because the pinouts page documents none of it:

- The **10 kΩ trim pot is a divider straight across `SIG` → `GND`**; the wiper feeds a 1 µF cap, then a 100 Ω resistor, then `IN_P`. So `SIG` carries a permanent 10 kΩ load, and the coupling cap sits _after_ the pot, not at the pin.
- **Gain is a fixed 24 dB.** `R12`/`R13` (100 Ω) are EMI series resistors, not gain-setting — this is the PAM8302**A**, the fixed-gain variant. The pot is the only hardware volume control.
- **`/SD` is tied to `VDD`.** There is no software mute; the amp is live whenever `RAW` is.

Consequence: **master volume belongs on the trim pot, not in software.** The amp's noise floor sits downstream of the pot and doesn't move, while every 6 dB of software attenuation costs a bit of a 10-bit DAC. Reserve software gain for expression within a sound.

Start the pot at minimum — 3.3 V p-p into a fixed 24 dB will clip hard.

Convenient: 2 kΩ of series R against the pot's 10 kΩ divides the 3.3 V swing to 2.75 V at `SIG`, inside Adafruit's stated 0–3 V input range with no extra divider.

JST PH 3-pin: **white = SIG, red = +, black = −**, same three signals as the large alligator/sew pads.

## Sources

- KB2040 pins — [`pins_arduino.h`](https://github.com/earlephilhower/arduino-pico/blob/master/variants/adafruit_kb2040/pins_arduino.h), [CircuitPython `pins.c`](https://github.com/adafruit/circuitpython/blob/main/ports/raspberrypi/boards/adafruit_kb2040/pins.c)
- Display 5394 — [product page](https://www.adafruit.com/product/5394) · [guide pinouts](https://learn.adafruit.com/adafruit-1-9-color-ips-tft-display/pinouts) · [PCB repo](https://github.com/adafruit/Adafruit-1.9in-320x170-Color-IPS-TFT-PCB) (`JP1` net list is the authority on pad order)
- Speaker 3885 — [product page](https://www.adafruit.com/product/3885) · [PCB repo / schematic](https://github.com/adafruit/Adafruit-STEMMA-Speaker-PCB)
- [NeoPixel Uberguide](https://learn.adafruit.com/adafruit-neopixel-uberguide)
