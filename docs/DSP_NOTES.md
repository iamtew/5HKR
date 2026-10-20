# DSP notes

- Per-lane kit (`laneKit0..7`): 707 / 808 (default) / 909 / Linn. Coefficient tables in `Voices`. Mix kits. Analog models, no sample ROM.
- One chain per lane: **0–6 oscillators** (squares free-run, sines reset per hit; optional pitch drop for knock) blended with **noise** (`noiseMix`) → **resonant TPT state-variable filter** (BP at unity peak, or HP; Hz + Q) → one-pole **tone LP** → 4 ms **click** burst → **snap + tail envelope** → gain → **tanh** (kit `sat` + Drive, normalised so peaks stay put) → optional **bit-round** after the envelope (gates tails like low-bit PCM).
- Spec times are ms to -60 dB, frequencies are Hz, so hits sound the same at any sample rate. Coefficients rebuild once per block when a knob, kit or tune changes; the per-sample path has no pow / exp / tan.
- Knobs at 0.5 = kit spec. Decay ×0.35..×2.8 tail. HPF ±2 octaves on the SVF. LPF ±2 octaves on the tone LP. Click 0..2× snap and click burst. Drive adds to kit sat. Accent: louder, more snap, SVF +15%.
- 808: six squares (205/304/370/523/540/800 Hz) into BP 7.1 kHz hats, 3.4–4.5 kHz cymbals; 540+800 Hz cowbell into BP 2.64 kHz with sharp snap over a 280 ms tail; clipped 2.5 kHz clave and 455+1667 Hz rim. 909: metal + noise hats through HP, 6-bit (`bits` 64) cymbals, rim with pitch drop and noise snap. 707: tight, 8-bit edge, darker LP, little sat. Linn: darkest LP, grainy, cabasa with snap, woody sine cow/rim with pitch knock.
- Accent peaks are normalised to about 0.8 at default knobs (saturated lanes sit a little hotter).
- Tune is a per-lane semitone ratio on oscillators and the SVF together.
- Swing delays odd 16ths: 0.5 = straight, 0.75 = half a step late.
- Per-lane loop wraps that lane against the pattern length (clave-in-5 against 16, etc.).
- Ducker: one-pole HP then LP on the input, envelope follower on that abs, gain `1 / (1 + amount * over * 12)` above thresh. Attack / release in ms. Listen replaces the output with the filtered key. Duck runs after FX + post EQ. Sidechain page FFT is that same filtered key.
- Lane mixer (before send split): trim dB, low-shelf 250 / peak 1k / high-shelf 4k, bipolar DJ filter, fader 0–1, constant-power pan, solo mask. Mute is still sequencer `laneOn`. Lane meters are post-fader peak, held ~400 ms, drawn -48..0 dBFS. Optional extra outs (buses 1–8) are that same post-strip stereo, no master Volume / FX / post EQ / duck.
- Master meter is max(|L|, |R|) after FX, post EQ and duck, same hold and scale. Clip is a sample at 0 dBFS.
- Lane FX send: dry = lane * (1 - send), send bus = lane * send. One global chain (JUCE Chorus / DelayLine+LP / Chorus-as-flanger / Phaser / Reverb). Order is packed `FX_ORDER` on APVTS state.
- Post EQ: 5 biquads (off/peak/shelves/HP/LP), same math as K1X. Spectrum FFT hop 2048 after the EQ. Same analyser type on the sidechain key.
- Not a sample ROM. If a hit sits off on a real interface, Tune is the calibration knob.
