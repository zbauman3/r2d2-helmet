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

### Pacing, checked against the films

Syllables sit around **150-400 ms** with real gaps between them. The first implementation used 90-200 ms with 40 ms gaps and read as a modem, not a droid. Recipe durations in `phrase.cpp` are roughly double that first pass.

**Not every syllable should move.** `dwell_pct` is the chance a syllable holds its anchor instead of travelling to another one. Forcing movement on every syllable — the original behaviour — makes phrases restless: constant back-and-forth with nothing ever landing. A held syllable still carries its overshoot, so it bends and settles rather than sitting flat, and that contrast is what gives the moving syllables somewhere to go.

Fewer syllables per press, too. The busiest recipes were saying too much for a single button push.

## Band limits

The output filter is **−3 dB at 7.0 kHz** and the speaker is a small enclosed 8 Ω unit that rolls off up there too. Do not put significant energy above ~7 kHz and expect it to arrive. Full response table is in the `hardware` skill.

### The real ceiling is ears, not electronics

Human hearing peaks in sensitivity between roughly **2 and 5 kHz** because of ear-canal resonance. A tone there is both louder and sharper than the same amplitude anywhere else, and at close range — which is where people stand when they talk to a costume — it is genuinely painful. Tested on hardware 2026-08-26 and confirmed: fundamentals around 4 kHz hurt.

Two mechanisms keep it out of that band, both in `config.h`:

**Soft ceiling.** `VOICE_F_KNEE` (1700 Hz) is where log-space compression starts; `VOICE_F_MAX` (3000 Hz) is an asymptote it approaches and never reaches. `soften()` in `phrase.cpp` does the squeezing. A hard `constrain()` was the original approach and it was wrong twice over — it flattened every excited gesture onto the same pinned tone, so the phrase lost its contour at exactly the moment it should have been most expressive. Compression keeps the shape and drops the pain.

| wants | gets |
| --- | --- |
| 1200 Hz | 1200 Hz (untouched) |
| 2200 Hz | 2030 Hz |
| 3000 Hz | 2258 Hz |
| 4200 Hz | 2410 Hz |
| 6000 Hz | 2515 Hz |

**Equal-loudness taper.** `VOICE_TILT_*` backs level off as pitch climbs, up to −40% at the ceiling. Applied at control rate in `voice.cpp`, so it tracks a sweep continuously rather than being fixed per syllable — a rising gesture fades slightly as it rises, which is also what a real voice does when it strains.

### Excitement must not be spent on pitch

The instinct is to convey agitation by raising pitch. Resist it: that is the one route to "excited" that also hurts. `AGITATION_PITCH_PCT` is capped at 12%. Everything else carries the load instead —

- **tempo**, hardest: `tighten()` compresses syllables and gaps to ~40% at full agitation
- **stutter**: repeat probability climbs with agitation, because repetition is what panic actually sounds like
- **wobble**: vibrato scales up, but only on cues that already wobble at rest — a calm cue should get *faster*, not develop a tremor

Note that `Alarm` is deliberately **not** the highest-pitched recipe. It earns its urgency from being clipped, repetitive and wobbling.

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

### The scream is not a synth patch

Everything else in R2's vocabulary descends from the ARP 2600 and is dominated by its fundamental. **The scream is Ben Burtt's own voice** — he screamed under a desk in the basement of Lucas's house, and it was sped up. Burtt's account: *"I just made a scream. I just screamed. Maybe there was some stress behind it."*

**It is therefore the one sound that is a recording, not synthesis.** `cue::Kind::Scream` plays `SCREAM_PCM` from flash via `sample.cpp` when that asset is present; everything else always goes through `voice.cpp`. Two oscillators and an envelope were never going to reproduce a human voice, and the scream is the only place in the vocabulary where that matters.

`src/scream_pcm.{h,cpp}` is **untracked and optional**. `audio.cpp` detects it with `__has_include` and falls back to the synthesised recipe below when it is absent, so a clone without it compiles and behaves sensibly. The interface those files must satisfy is documented in `.gitignore`.

The synthesised `Scream` recipe in `phrase.cpp` is kept both as that fallback and to keep the recipe table index-aligned with `cue::Kind`. Its three defining traits:

- **Harmonically rich** (`harm 60`, against 8–25 for everything else). A voice has real harmonic content; this is the single biggest thing separating a scream from a whistle.
- **Fast vibrato** (~13 Hz). Natural vocal vibrato is around 5 Hz — tape-speeding it up is what lands it there.
- **An arc, not a sweep.** The `arc` flag makes the syllable leap to its peak in the first fifth and spend the remaining four fifths falling. That is the shape of a voice; overshoot-and-settle is the shape of a servo, and no anchor-hop contour produces it.

The scream also **interrupts** whatever is playing rather than queueing behind it, and pins arousal at maximum for `SCREAM_PIN_MS` so the screen, PSIs and periscope go frantic alongside it.

Source: [Soundeffects Wiki — Skywalker Astromech Critical Scream](https://soundeffects.fandom.com/wiki/SKYWALKER_ASTROMECH_CRITICAL_SCREAM)

### Deliberate answers

`Affirm` and `Decline` are never chosen at random. They sit past `cue::END_SELECTABLE`, so the weight tables in `interaction.cpp` stop short of them and they only fire when requested by name via `interaction::answer()`.

Neither escalates agitation or updates `last_kind` — an answer is outside the random rotation and should not block whatever neutral cue comes next. It does read current agitation, so a wound-up "no" comes out sharper than a calm one.

The recipes lean on `terminal` (+1 lifts the final syllable, −1 drops it), which is what carries yes-ness and no-ness. `Affirm` has no vibrato and almost no harmonic content, because wobble reads as uncertainty and an answer has to sound sure of itself. `Decline` carries enough harmonic content to sound flat and unimpressed rather than merely sad.

### Arousal collapses; it does not glide

`level()` returns zero once `AROUSAL_HOLD_MS` (600 ms) has passed since the last press. There is no decay tail. Escalating requires *sustained* pressing and ends the moment you stop, which makes the level something you steer rather than something you wait out.

The same constant is the escalation window — deliberately one number. A press arriving after the collapse starts fresh from zero, which is the only self-consistent reading: if the level is already gone there is nothing left to build on.

**Anything that reflects a flurry after the fact must LATCH the level it saw.** `interaction.cpp` keeps `pending_level` for exactly this: a throttled utterance typically fires a second or more after the press that queued it, so re-reading arousal at fire time would reliably answer "calm" and a frantic flurry would come out as a mild chirp — visible on the screen, absent from the voice.

### Presses are throttled, not queued

A trigger press while R2 is mid-utterance does **not** interrupt him and does **not** stack up. It raises arousal and sets a single pending bit; whatever the press count, exactly one follow-up fires when the current utterance finishes, chosen at the arousal level reached *by then*.

Cutting an utterance short to start the next was the original behaviour and it made a flurry sound like stuttering machinery rather than someone getting steadily more worked up.

Core0 learns when to release the pending press from core1's `Finished` reply on the return FIFO. That reply fires when the last sample has been *generated*, roughly a buffer (~46 ms) before it is heard — which is what makes consecutive utterances butt together seamlessly instead of leaving a buffer-length hole.

### Pattern selection — built, in `interaction.cpp`

Core0 policy, not a uniform random pick. Two things shape it:

**Agitation.** Presses within `ESCALATE_WINDOW_MS` of each other escalate, so hammering the button reads as one rising exchange rather than independent samples. It decays on its own at `AGITATION_DECAY_PER_SEC`, lazily — the value is only ever read at the moment of a press, so nothing ticks it in `loop()`. Three weight bands (calm / roused / agitated) shift the register: calm R2 chatters and muses, wound-up R2 alarms and snaps. `Grumble` survives at every level because being put upon is his baseline.

**Memory.** The cue that just fired is zeroed out of the next choice. Done by adjusting the weights rather than re-rolling until it differs, so a heavily-weighted cue in a lopsided band cannot spin.

The escalation idea generalises and is worth reaching for elsewhere: the user specifically asked that other features be built with the same mentality.

### Arousal paces the whole costume, not just the voice

`interaction::pace(calm_ms, agitated_ms, now_ms)` interpolates any subsystem's own two timings by the current arousal. The display's churn rate and the PSI flip rate both go through it, so a flurry of presses speeds up the screen and the lights along with the speech, and everything eases back down together as agitation decays.

Each subsystem supplies **different endpoints on purpose**. Matching them exactly reads as one clock driving everything, which is the opposite of alive.

`agitation()` must stay a **pure function** of elapsed time. It is now sampled every pass through `loop()` by several modules; the original implementation decayed by subtracting on each call without advancing its reference time, which was harmless when only the trigger read it and would collapse to zero within milliseconds once anything else did.

## Where this runs

Core1, always. `PWMAudio::begin()` and all synthesis live in `setup1()`/`loop1()`. See the `firmware` skill for why — it is not optional, and it is what makes the NeoPixel interrupt-masking bug harmless.

## Sources

- [How the ARP 2600 created R2-D2](https://www.redbull.com/ca-en/playing-with-arp-2600-star-wars-r2d2)
- [Ben Burtt Special: Star Wars – The Sounds](https://designingsound.org/2009/09/10/ben-burtt-special-star-wars-the-sounds-part-2/)
- [arduino-pico PWMAudio docs](https://arduino-pico.readthedocs.io/en/latest/pwm.html) · [PWMAudio.cpp](https://github.com/earlephilhower/arduino-pico/blob/master/libraries/PWMAudio/src/PWMAudio.cpp)
