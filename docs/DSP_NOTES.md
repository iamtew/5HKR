# DSP notes

- Hats / crash / cym / shaker: LCG noise through a one-pole HP, exponential env. Accent is louder and a bit brighter (higher HP).
- Clave: decaying sine ~2450 Hz.
- Cowbell: 540 Hz + 800 Hz sines.
- Rim: 380 Hz sine plus a short noise click.
- Tune is a per-lane semitone ratio on those frequencies / HP.
- Swing delays odd 16ths: 0.5 = straight, 0.75 = half a step late.
- Per-lane loop wraps that lane against the pattern length (clave-in-5 against 16, etc.).
- Ducker: envelope follower on input abs, gain `1 / (1 + amount * over * 12)` above thresh. Attack / release in ms.
- Not a sample ROM. If a hit sits off on a real interface, Tune is the calibration knob.
