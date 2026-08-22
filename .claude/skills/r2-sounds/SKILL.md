---
name: r2-sounds
description: How R2-D2's voice was actually created, the rules for synthesizing it convincingly, and the PWMAudio configuration and constraints on this board — use when writing, tuning, or debugging any audio or sound-synthesis code.
---

# R2's voice

## What it actually is

The common claim that the voice is "swept tones from an ARP 2600" is incomplete in a way that matters for synthesis. Ben Burtt blended **his own voice** with the ARP patch: he fed a microphone through the synth's self-resonating filter and performed lines live while twiddling the filter. A sample-and-hold LFO through that resonant filter produced the chirps; the human input produced the **phrasing**.

The practical consequence: any purely synthetic result is missing a vocal/formant layer, and the thing that reads as "R2" is the phrasing, not the timbre. Spend effort on gesture and timing before spending it on waveform quality.

## Synthesis rules

Three rules recover most of that phrasing algorithmically:

1. **Sweeps are exponential, not linear.** Pitch glides in log-frequency space. A linear ramp sounds like a test tone; an exponential one sounds like speech.
2. **Two or three random anchor pitches per utterance**, not a continuous random walk. Pick anchors, then sweep between them. This is what gives phrases their sense of shape and intent.
3. **Overshoot before settling.** Arriving at a target pitch by slightly passing it and falling back is the single most characteristic gesture. Without it everything sounds mechanical.

Beyond that: vary utterance length, leave real silence between phrases, and let a small number of "words" repeat within an utterance — R2 stutters and doubles back.

## Band limits

The output filter is **−3 dB at 7.0 kHz** and the speaker is a small enclosed 8 Ω unit that rolls off up there too. Do not put significant energy above ~7 kHz and expect it to arrive. Fundamentals in the 300 Hz – 5 kHz range with their harmonics are the usable band. Full response table is in the `hardware` skill.

## PWMAudio configuration

```cpp
// Carrier MUST be set explicitly. PWMAudio's constructor defaults _freq to 48000
// — a 48 kHz carrier, barely above the audio band.
audio.begin(/*sampleRate*/ 22050, /*carrierHz*/ 130000);   // 133 MHz / 1024 => 10-bit
```

- Samples are **signed 16-bit**, auto-scaled down to the PWM hardware resolution.
- Pacing is a **DMA timer** (`dma_claim_unused_timer`), not a CPU interrupt. There is no per-sample ISR.
- Refills come per buffer via an `onTransmit()` callback.
- Mono works on any pin. Stereo needs an even pin (the odd sibling becomes right).
- `setPWMFrequency()` derives wrap as `sys_clk / freq` — and `sys_clk` here is **133 MHz**, not 125.

### Never use a ~260 kHz carrier

The PAM8302A's own class-D output switches at **~250 kHz**. A 260 kHz carrier beats against it at **10 kHz**, dead centre of the audio band. That rules out 9-bit resolution at 133 MHz.

| Carrier | Resolution | Verdict |
| --- | --- | --- |
| 129.9 kHz | 10-bit | **Used here.** Difference product lands at 120 kHz |
| 259.8 kHz | 9-bit | **Forbidden.** 10 kHz beat |
| 519.5 kHz | 8-bit | Safe and easier to filter, at 48 dB dynamic range |

If the top end sounds too dull, the better fix is 8-bit at 520 kHz *plus* 4.7 nF filter caps — that buys a flat audio band and good carrier rejection together. 8-bit is adequate for synthesized bleeps.

## Volume policy

**Master volume is the trim pot on the speaker board, not software.** Gain there is a fixed 24 dB and the pot is a divider ahead of it, so the amp's noise floor does not move with the pot — but the PWM DAC only has 10 bits, and every 6 dB of software attenuation throws one away.

Drive the PWM near full scale. Use software gain only for expression *within* a sound — an utterance trailing off, a nervous quiet mutter — never as a master level.

There is also no software mute: `/SD` is tied to `VDD` on the STEMMA board, so the amp is live whenever `RAW` is.

## What triggers a sound

**Only button 2** (`D3`/GP3). Active mode — button 1 — animates the screen, lights, and periscope and is *deliberately silent*. There is no autonomous chirping, by design: sound is manual and deliberate, which is both more controllable in person and kinder to everyone standing nearby for an evening.

Button 2 fires in either mode, so noise does not require lighting up first.

### Pattern selection — not designed yet

Which interaction fires on a press is meant to be **pattern-driven**, not a uniform random pick from a bag of sounds. That design is open. Things worth weighing when it gets built:

- Repeated presses in quick succession should read as one escalating exchange, not as independent samples.
- Some memory of what just played, so the same utterance does not come back immediately.
- Mode probably matters: a press in standby might warrant something shorter or grumpier than one in active mode.
- Length variation carries more character than timbre variation — see the phrasing rules above.

Do not build a placeholder random picker and call it done; the pattern logic *is* the personality.

## Where this runs

Core1, always. `PWMAudio::begin()` and all synthesis live in `setup1()`/`loop1()`. See the `firmware` skill for why — it is not optional, and it is what makes the NeoPixel interrupt-masking bug harmless.

## Sources

- [How the ARP 2600 created R2-D2](https://www.redbull.com/ca-en/playing-with-arp-2600-star-wars-r2d2)
- [Ben Burtt Special: Star Wars – The Sounds](https://designingsound.org/2009/09/10/ben-burtt-special-star-wars-the-sounds-part-2/)
- [arduino-pico PWMAudio docs](https://arduino-pico.readthedocs.io/en/latest/pwm.html) · [PWMAudio.cpp](https://github.com/earlephilhower/arduino-pico/blob/master/libraries/PWMAudio/src/PWMAudio.cpp)
