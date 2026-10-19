# DSP notes

- Hats / crash / cym / shaker: LCG noise through a one-pole HP, exponential env. Accent is louder and a bit brighter (higher HP).
- Clave: decaying sine ~2450 Hz.
- Cowbell: 540 Hz + 800 Hz sines.
- Rim: 380 Hz sine plus a short noise click.
- Tune is a per-lane semitone ratio on those frequencies / HP.
- Swing delays odd 16ths: 0.5 = straight, 0.75 = half a step late.
- Per-lane loop wraps that lane against the pattern length (clave-in-5 against 16, etc.).
- Ducker: one-pole HP then LP on the input, envelope follower on that abs, gain `1 / (1 + amount * over * 12)` above thresh. Attack / release in ms. Listen replaces the output with the filtered key. Duck runs after FX + post EQ.
- Lane FX send: dry = lane * (1 - send), send bus = lane * send. One global chain (JUCE Chorus / DelayLine+LP / Chorus-as-flanger / Phaser / Reverb). Order is packed `FX_ORDER` on APVTS state.
- Post EQ: 5 biquads (off/peak/shelves/HP/LP), same math as K1X. Spectrum FFT hop 2048 after the EQ.
- Not a sample ROM. If a hit sits off on a real interface, Tune is the calibration knob.
