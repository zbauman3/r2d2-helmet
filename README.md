# r2d2-helmet

A project for an R2D2 costume helmet.

(pls dont sue me, Mr. Mouse)

---

## Hardware

| Part                   | PID  | Role                                  |
| ---------------------- | ---- | ------------------------------------- |
| Adafruit KB2040        | 5302 | RP2040 MCU, Pro Micro form factor     |
| Breadboard NeoPixel ×5 | 1312 | SK6812, dome lighting behind diffuser |
| STEMMA Speaker         | 3885 | PAM8302A class-D + 1 W 8 Ω speaker    |
| 1.9" 320×170 IPS TFT   | 5394 | ST7789, logic display panel           |
| EYESPI Breakout        | 5613 | Required — 5394 has no 0.1" header    |
| EYESPI Cable 50 mm     | 5462 | 18-pin 0.5 mm FPC, A-B type           |

Passives: 2× 1 kΩ, 2× 10 nF, 1× 470 Ω, 1× 100 µF electrolytic, 1× 470 µF electrolytic.

### Pin assignments

Verified against `variants/adafruit_kb2040/pins_arduino.h` (arduino-pico) and
`ports/raspberrypi/boards/adafruit_kb2040/pins.c` (CircuitPython). Both agree.

| Silkscreen | GPIO | PWM slice | Used for                  |
| ---------- | ---- | --------- | ------------------------- |
| D8         | GP8  | 4A        | Audio PWM out → RC filter |
| D5         | GP5  | 2B        | NeoPixel data             |
| D6         | GP6  | 3A        | Periscope servo (later)   |
| D7         | GP7  | 3B        | TFT D/C                   |
| D4         | GP4  | 2A        | TFT reset                 |
| D10        | GP10 | 5A        | TFT chip select           |
| D9         | GP9  | 4B        | SD chip select (optional) |
| SCK        | GP18 | 1A        | TFT clock (SPI0)          |
| MOSI       | GP19 | 1B        | TFT data (SPI0)           |
| MISO       | GP20 | 2A        | SD data out (optional)    |
| A2         | GP28 | 6A        | TFT backlight PWM         |

Unused and free: D0/D1 (GP0/GP1, Serial1), D2/D3 (GP2/GP3, Wire1), A0/A1/A3
(GP26/GP27/GP29), STEMMA QT port (GP12/GP13). GP11 is the onboard BOOT button and
GP17 is the onboard NeoPixel — neither is castellated.

`PWMAudio` claims all of PWM slice 4, so **GP9 must stay digital-only** — no
`analogWrite()` on it.

---

## Wiring

### Board pin geometry

The KB2040 is Pro Micro shaped: 12 castellated pads per edge, USB-C at the top.

```
              USB-C
        ┌───────────────┐
  D0/TX │1           24│ RAW      <- 5V from USB, fused
  D1/RX │2           23│ GND
    GND │3           22│ RST
    GND │4           21│ 3V       <- 3.3V reg out, 500 mA
     D2 │5           20│ A3
     D3 │6           19│ A2       <- TFT backlight
     D4 │7           18│ A1
     D5 │8           17│ A0
     D6 │9           16│ SCK
     D7 │10          15│ MISO
     D8 │11          14│ MOSI
     D9 │12          13│ D10
        └───────────────┘
         STEMMA QT on end (GP12/GP13)
```

### Full schematic

```mermaid
flowchart TB

  %% ============ POWER ============
  subgraph PWR["⚡ Power rails"]
    direction TB
    USB["USB-C<br/>5 V in"]
    RAW["RAW pin<br/><b>5 V</b> · fused 500 mA–1 A"]
    V3["3V pin<br/><b>3.3 V</b> · 500 mA total<br/><i>incl. the MCU itself</i>"]
    GND(["GND<br/><b>common — tie all together</b>"])
    USB --> RAW
    RAW -->|"onboard LDO"| V3
  end

  %% ============ MCU ============
  subgraph MCU["🧠 Adafruit KB2040 · RP2040 @ 133 MHz"]
    direction TB
    P_D8["<b>D8</b> · GP8<br/>PWM slice 4A"]
    P_D5["<b>D5</b> · GP5"]
    P_SCK["<b>SCK</b> · GP18"]
    P_MOSI["<b>MOSI</b> · GP19"]
    P_MISO["<b>MISO</b> · GP20"]
    P_D7["<b>D7</b> · GP7"]
    P_D4["<b>D4</b> · GP4"]
    P_D10["<b>D10</b> · GP10"]
    P_D9["<b>D9</b> · GP9"]
    P_A2["<b>A2</b> · GP28<br/>PWM slice 6A"]
  end

  %% ============ AUDIO ============
  subgraph AUD["🔊 Audio chain"]
    direction LR
    R1["R1<br/>1 kΩ"]
    N1(("node A"))
    C1["C1<br/>10 nF"]
    R2["R2<br/>1 kΩ"]
    N2(("node B"))
    C2["C2<br/>10 nF"]
    CB1["C_bulk<br/>470 µF"]
    subgraph SPK["STEMMA Speaker 3885 · PAM8302A"]
      direction TB
      S_SIG["<b>SIG</b> pad / white wire<br/>0–3 V in · AC-coupled 1.0 µF onboard"]
      S_VCC["<b>+</b> pad / red wire<br/>3–5 V"]
      S_GND["<b>−</b> pad / black wire"]
      S_POT["🔧 volume trim pot<br/><b>start at minimum</b>"]
      S_OUT["1 W 8 Ω speaker<br/>~250 kHz class-D switching"]
      S_SIG --> S_POT --> S_OUT
    end
  end

  P_D8 -->|"PWM carrier 130 kHz<br/>22.05 kS/s"| R1
  R1 --> N1
  N1 --> C1
  C1 --> GND
  N1 --> R2
  R2 --> N2
  N2 --> C2
  C2 --> GND
  N2 -->|"filtered audio<br/>2-pole, fc ≈ 15.9 kHz"| S_SIG
  RAW -->|"5 V"| S_VCC
  RAW --> CB1
  CB1 --> GND
  S_GND --> GND

  %% ============ NEOPIXELS ============
  subgraph NPX["💡 NeoPixel chain · 5× SK6812"]
    direction LR
    R3["R3<br/>470 Ω"]
    CB2["C_bulk<br/>100 µF"]
    NP1["Pixel 1<br/>DIN→DOUT"]
    NP2["Pixel 2"]
    NP3["Pixel 3"]
    NP4["Pixel 4"]
    NP5["Pixel 5<br/><i>DOUT unused</i>"]
    NP1 -->|"DOUT→DIN"| NP2
    NP2 -->|"DOUT→DIN"| NP3
    NP3 -->|"DOUT→DIN"| NP4
    NP4 -->|"DOUT→DIN"| NP5
  end

  P_D5 -->|"800 kHz data<br/>3.3 V logic"| R3
  R3 -->|"to DIN of pixel 1"| NP1
  V3 -->|"<b>3.3 V — not 5 V</b><br/>drops V_IH to 2.31 V"| CB2
  CB2 --> NP1
  CB2 --> GND

  %% ============ DISPLAY ============
  subgraph DISP["🖥️ Logic display"]
    direction TB
    subgraph EYE["EYESPI Breakout 5613 · 0.1in header"]
      direction TB
      E_VIN["<b>Vin</b>"]
      E_GND["<b>Gnd</b>"]
      E_SCK["<b>SCK</b>"]
      E_MOSI["<b>MOSI</b>"]
      E_MISO["<b>MISO</b>"]
      E_DC["<b>DC</b>"]
      E_RST["<b>RST</b>"]
      E_TCS["<b>TCS</b>"]
      E_SDCS["<b>SDCS</b>"]
      E_LITE["<b>Lite</b>"]
    end
    TFT["<b>Adafruit 5394</b><br/>1.9in 320×170 IPS · ST7789<br/>visible area 46 × 25 mm<br/>onboard 3.3 V LDO + level shifter<br/>microSD slot"]
    EYE ==>|"18-pin 0.5 mm FPC<br/>EYESPI cable 5462"| TFT
  end

  P_SCK  --> E_SCK
  P_MOSI --> E_MOSI
  P_MISO -.->|"optional · microSD only"| E_MISO
  P_D7   --> E_DC
  P_D4   --> E_RST
  P_D10  --> E_TCS
  P_D9   -.->|"optional · microSD only"| E_SDCS
  P_A2   -->|"PWM dimming"| E_LITE
  V3     -->|"3.3 V"| E_VIN
  E_GND  --> GND

  classDef pwr fill:#4a2c00,stroke:#ffa726,stroke-width:2px,color:#ffe0b2
  classDef mcu fill:#00332b,stroke:#26a69a,stroke-width:2px,color:#b2dfdb
  classDef aud fill:#3a0d2e,stroke:#ec407a,stroke-width:2px,color:#f8bbd0
  classDef npx fill:#0d2137,stroke:#42a5f5,stroke-width:2px,color:#bbdefb
  classDef dsp fill:#1a2e05,stroke:#9ccc65,stroke-width:2px,color:#dcedc8
  class USB,RAW,V3,GND pwr
  class P_D8,P_D5,P_SCK,P_MOSI,P_MISO,P_D7,P_D4,P_D10,P_D9,P_A2 mcu
  class R1,R2,C1,C2,N1,N2,CB1,S_SIG,S_VCC,S_GND,S_POT,S_OUT aud
  class R3,CB2,NP1,NP2,NP3,NP4,NP5 npx
  class E_VIN,E_GND,E_SCK,E_MOSI,E_MISO,E_DC,E_RST,E_TCS,E_SDCS,E_LITE,TFT dsp
```

### Connection table

Wire it from this, not from the diagram — the diagram shows topology, this shows nets.

**Audio**

| From               | To                                           |
| ------------------ | -------------------------------------------- |
| KB2040 `D8` (GP8)  | R1 (1 kΩ) leg 1                              |
| R1 leg 2           | node A — R2 leg 1, and C1 (10 nF) leg 1      |
| C1 leg 2           | GND                                          |
| R2 leg 2           | node B — C2 (10 nF) leg 1, and speaker `SIG` |
| C2 leg 2           | GND                                          |
| KB2040 `RAW` (5 V) | speaker `+` (red), and C_bulk 470 µF `+`     |
| C_bulk 470 µF `−`  | GND                                          |
| KB2040 `GND`       | speaker `−` (black)                          |

STEMMA Speaker JST PH 3-pin: **white = SIG, red = +, black = −**. The same three
signals are on the large alligator/sew pads, labelled `SIG` / `+` / `−`.

**NeoPixels**

| From              | To                                                      |
| ----------------- | ------------------------------------------------------- |
| KB2040 `D5` (GP5) | R3 (470 Ω) leg 1                                        |
| R3 leg 2          | pixel 1 `DIN`                                           |
| KB2040 `3V`       | pixel 1..5 `+` (all in parallel), and C_bulk 100 µF `+` |
| KB2040 `GND`      | pixel 1..5 `−` (all in parallel), and C_bulk 100 µF `−` |
| pixel N `DOUT`    | pixel N+1 `DIN`                                         |
| pixel 5 `DOUT`    | leave unconnected                                       |

Each pixel has 3 pads per side; the two sides mirror each other except that data
is `DIN` on one side and `DOUT` on the other. Power and ground are common across
both sides, so you can daisy-chain in either direction.

**Display** — via EYESPI breakout, wire by silkscreen label:

| EYESPI breakout | KB2040                                |
| --------------- | ------------------------------------- |
| `Vin`           | `3V`                                  |
| `Gnd`           | `GND`                                 |
| `SCK`           | `SCK` (GP18)                          |
| `MOSI`          | `MOSI` (GP19)                         |
| `DC`            | `D7` (GP7)                            |
| `RST`           | `D4` (GP4)                            |
| `TCS`           | `D10` (GP10)                          |
| `Lite`          | `A2` (GP28)                           |
| `MISO`          | `MISO` (GP20) — only if using microSD |
| `SDCS`          | `D9` (GP9) — only if using microSD    |

Leave `SDA`, `SCL`, `GP1`, `GP2`, `TSCS`, `MEMCS`, `BUSY`, `INT` unconnected —
this display has no touch panel, no onboard RAM, and is not eInk.

`Lite` can go straight to `3V` if you don't want software brightness control.

---

## Things that will bite you

**Tie every ground together.** MCU, speaker, pixels, display. The most common
cause of a PWM audio path that hums or does nothing is a floating amp ground.

**Start the speaker trim pot at minimum.** The PAM8302A has gain and the RC
network doesn't attenuate much in-band. A 3.3 V p-p signal into a 1 W speaker with
the pot up will clip hard.

**The 470 µF at the amp is not optional.** Class-D current spikes on transients sag
`RAW` enough to brown out the MCU. It shows up as random resets that only appear
once audio and lights run together, which is a miserable thing to debug later.

**Power the pixels from `3V`, not `RAW`.** SK6812 needs a data high of 0.7 × VDD.
At 5 V that's 3.5 V and the RP2040 only puts out 3.3 V — marginal. At 3.3 V the
threshold drops to 2.31 V and there's plenty of margin. Slightly dimmer output,
which is fine behind a diffuser.

**3.3 V rail budget:** the regulator supplies 500 mA _including the MCU_. Five
pixels at full white plus the display's backlight is roughly 260 mA of that. Fine
as specified — just don't add a second string to this rail.

**Support the display panel mechanically.** Adafruit's own warning: 5394 was
designed for smartwatches where cover glass retains it, and "without something
gently holding the screen down, the backlight can eventually peel away from the
TFT." The dome faceplate needs to press lightly on the display face.

**Check for solder pads before ordering the EYESPI parts.** Every Adafruit source
for 5394 documents only the 18-pin FPC connector, but look at the physical board —
if it has a 0.1" header row you can skip 5613 and 5462 entirely and wire direct.

See [`docs/wiring.md`](docs/wiring.md) for the parts research and
[`docs/hardware-notes.md`](docs/hardware-notes.md) for the toolchain and audio
findings.
