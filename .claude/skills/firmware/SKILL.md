---
name: firmware
description: PlatformIO configuration, the earlephilhower arduino-pico core, build/flash/monitor workflow, library choices, and the dual-core concurrency model for this RP2040 project — use when building or flashing, editing platformio.ini, adding a library, or deciding what runs on which core.
---

# Firmware and toolchain

## We are using Arduino

The choice was never Arduino vs. not-Arduino. It is **earlephilhower's `arduino-pico` core** vs. **Arduino's official Mbed core**, and we picked the former. Arduino libraries (Adafruit_ST7789, Adafruit_NeoPixel, PWMAudio) are expected and correct.

Why not Mbed: Arm ended Mbed OS support in **July 2026**. Arduino's replacement path is Zephyr, not a fixed Mbed RP2040 core. Philhower's core is also the one that exposes `setup1()`/`loop1()`, which this design depends on.

## platformio.ini

```ini
platform  = https://github.com/maxgerhardt/platform-raspberrypi.git
board     = adafruit_kb2040
framework = arduino
```

**The official `platformio/platform-raspberrypi` will not work.** Verified against `platform.json` on `develop` @ 1.20.0: its frameworks block lists only `framework-arduino-mbed`. Claims that mainline "now supports" the Philhower core are wrong; the arduino-pico docs still direct users to maxgerhardt's fork.

Two details that are easy to get wrong:

- The board id is **`adafruit_kb2040`**, not `kb2040`.
- **`board_build.core` is unnecessary** — that board JSON already hardcodes `"core": "earlephilhower"` and `"f_cpu": "133000000L"`. It is only needed for generic ids like `pico`.

`board_build.filesystem_size = 0.5m` reserves LittleFS. Currently unused — there is no microSD and sounds are synthesized, so nothing needs storage yet.

The same board id declares `"frameworks": ["arduino", "picosdk"]`, so dropping to the bare Pico SDK later is a one-line change. Arduino libraries would not survive that switch.

## Clock

The board definition runs at **133 MHz**, not 125 MHz. Every derived number — PWM carrier for a given resolution, in particular — must use 133 MHz. Above 125 MHz the SDK also switches `peri_clk` feeding SPI/UART to 48 MHz.

| PWM wrap | Resolution | Carrier @ 133 MHz |
| --- | --- | --- |
| 256 | 8-bit | 519.5 kHz |
| 512 | 9-bit | 259.8 kHz — **never use, see `r2-sounds`** |
| 1024 | 10-bit | 129.9 kHz — what this build uses |

## Build, flash, monitor

```sh
pio run                    # build
pio run -t upload          # build + flash
pio device monitor         # serial, 115200
```

**`pio` is not on `PATH` on this machine.** It ships inside the VS Code extension's virtualenv — use `~/.platformio/penv/bin/pio` or add that directory to `PATH`.

The KB2040 has no physical reset button broken out for BOOTSEL. To force the bootloader: **double-tap the RST pad to GND**, or hold BOOT (GP11) while resetting. It enumerates as a `RPI-RP2` mass-storage volume; `pio run -t upload` copies a `.uf2` to it.

If a bad build bricks the serial port, the double-tap always works — the bootloader lives in ROM.

## Libraries

```ini
lib_deps =
    adafruit/Adafruit NeoPixel
    adafruit/Adafruit GFX Library
    adafruit/Adafruit ST7735 and ST7789 Library
    adafruit/Adafruit BusIO
```

`PWMAudio` and `Servo` both ship with the core — do not add either to `lib_deps`.

`Servo` is PIO-backed, so the pulse train is generated in hardware and keeps running with no CPU attention. That also means the `noInterrupts()` inside `NeoPixel::show()` cannot glitch it. Two traps in `attach()`, both silent:

- `DEFAULT_MIN/MAX_PULSE_WIDTH` are 1000/2000 µs, roughly **half** an SG92R's travel. Pass the real range.
- The 3-argument `attach(pin, min, max)` forwards `_valueUs`, which the constructor initialises to `DEFAULT_NEUTRAL_PULSE_WIDTH` (1500 µs) — mid-travel. Attaching commands that position immediately, so the servo slams from wherever it physically rests to 90° at full speed, through whatever the horn is connected to. **Always use the 4-argument form** and pass the angle you believe you are already at.

## Concurrency model

**This is the load-bearing decision of the whole firmware.**

| Core | Owns |
| --- | --- |
| **core1** (`setup1()` / `loop1()`) | `PWMAudio` and all sound synthesis. Nothing else. |
| **core0** (`setup()` / `loop()`) | Display, NeoPixels, both buttons, mode state, behaviour logic. |

Two independent reasons, both fatal if ignored:

1. **The display blocks for longer than the audio buffer.** A full-screen 320×170×2 B blit is ~109 KB — about 36 ms of blocking SPI at 24 MHz. PWMAudio's default buffering is roughly 23 ms. Synthesis in `loop()` underruns on every redraw.
2. **`Adafruit_NeoPixel::show()` masks interrupts.** RP2040 is *not* in the library's exclusion list, so `noInterrupts()` runs, and `rp2040Show()` feeds the PIO FIFO with `pio_sm_put_blocking()` one byte at a time — roughly `numBytes × 10 µs`. Calling `PWMAudio::begin()` from core1 installs its DMA IRQ on core1's NVIC, and `PRIMASK` is per-core on Cortex-M0+, so core0's blackout cannot reach it.

At two pixels the blackout is only ~60 µs, so reason 2 is currently mild — but it scales linearly if the dome ever gets a real pixel ring, and reason 1 is unavoidable regardless.

Corollary: the doc-era mitigation "drive the NeoPixels from PIO" is a no-op. They already are. (See Adafruit_NeoPixel issue #441, open since July 2025.)

### Crossing between cores

Keep the interface narrow — ideally a single "what should I be playing" value that core0 writes and core1 reads. Anything wider needs `mutex_t` or the SIO FIFO. Do not share Arduino `String`, dynamic allocation, or library objects across cores.

## SPI

`pins_arduino.h` defines `PIN_SPI0_SCK = 18`, `MOSI = 19`, `MISO = 20` — exactly the wiring — so the default `SPI` object works with **no `setSCK()`/`setTX()` calls**. `Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST)` is enough.

`SPI_HOWMANY` is 1: this board pins out only one SPI bus.

**Start at 8–12 MHz** and raise once the display is stable. 24 MHz over breadboard jumper wires is optimistic.

## Resource budget

| Resource | Total | Claimed |
| --- | --- | --- |
| PIO state machines | 8 | 1 by NeoPixel, 1 by `Servo` (periscope) |
| PWM slices | 8 (2 ch. each) | slice 4 entirely, by `PWMAudio` |
| DMA | 12 channels + timers | channels *and* a DMA timer, by `PWMAudio` |
| GPIO | 20 (18 castellated + 2 on STEMMA QT) | 11 used |

A PIO-driven display would not be free. Pin assignments and slice overlaps are in the `hardware` skill.
