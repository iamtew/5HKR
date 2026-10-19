# DSP notes

- Per-lane kit (`laneKit0..7`): 707 / 808 (default) / 909 / Linn. Coefficient tables in `Voices`. Mix kits. Analog models, no sample ROM.
- Three generators: **noise** (LCG through HP, optional one-pole LP = crude bandpass, optional bit-round), **metal** (4–6 squares/sines then HP), **tone** (1–2 osc + click). Accent is louder and a bit brighter (higher HP). Click is `env^2 * noise`.
- 808: square-stack hats/crash/cym (classic inharmonic ratios), 540+800 Hz square cowbell. 909: gated bandpassed-noise hats, sizzle crash, ringing ride. 707: short thin bandpass, damped cow/clave. Linn: darker noise, grainy cabasa shaker, woody cow/rim.
- Tune is a per-lane semitone ratio on those frequencies / HP.
- Swing delays odd 16ths: 0.5 = straight, 0.75 = half a step late.
- Per-lane loop wraps that lane against the pattern length (clave-in-5 against 16, etc.).
- Ducker: one-pole HP then LP on the input, envelope follower on that abs, gain `1 / (1 + amount * over * 12)` above thresh. Attack / release in ms. Listen replaces the output with the filtered key. Duck runs after FX + post EQ. Sidechain page FFT is that same filtered key.
- Lane FX send: dry = lane * (1 - send), send bus = lane * send. One global chain (JUCE Chorus / DelayLine+LP / Chorus-as-flanger / Phaser / Reverb). Order is packed `FX_ORDER` on APVTS state.
- Post EQ: 5 biquads (off/peak/shelves/HP/LP), same math as K1X. Spectrum FFT hop 2048 after the EQ. Same analyser type on the sidechain key.
- Not a sample ROM. If a hit sits off on a real interface, Tune is the calibration knob.
