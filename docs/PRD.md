# 5HKR – Brief

## Vision

A perc groove box: eight tiny analog-ish synth lanes on a 16th grid, with banks you can key-lock like TEW03. A lane can play one loaded one-shot instead of its synth.

## MVP

- Eight lanes: CHH, OHH, Crash, Cym, Shaker, Clave, Cow, Rim
- 16th-note sequencer, 16 / 32 / 64 steps, per-lane loop length and swing
- Per-step off / on / accent
- 3 banks × 12 patterns, hold C1 (Ableton C0) for bank 1 pattern 1
- Per-lane Tune and mute
- Per-lane one-shot sample (last kit choice), stored in the patch
- Sidechain duck from the audio input
- Patch + bank library (`.5hkrp` / `.5hkrb`)
- Editor pages: Main, Mixer, Effects, EQ, Sidechain
- Per-lane mixer: Gain, High/Mid/Low, DJ filter, pan, mute, solo, fader
- Per-lane FX send into a global delay / reverb / chorus / flanger / phaser chain
- Post-effects 5-band parametric EQ
- VST3 + Standalone
- Magenta / cyan chassis, logo, MIT

## Non-goals

- Sample library, streaming, multi-sample instruments
- MIDI out of the sequence
- Comp / dist / filter / insert-EQ inside the FX rack
