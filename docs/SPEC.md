# 5HKR spec

This file is the product spec.

Percussion overlay: eight analog-ish synth lanes on a 16th grid, with banks you can key-lock like TEW03. Stupid Audio. MIT. Not a sample library.

- A lane can play one loaded one-shot instead of its synth
- Overlay runs while a mapped MIDI key is held
- Not a sample library, not streaming, not a multi-sample instrument, no MIDI out of the sequence

## Product

- VST3 + Standalone
- About + GitHub tag check (title-bar name + version)
- Window scale saved with the session
- Patches (`.5hkrp`) — knobs. `%AppData%\Stupid Audio\5HKR\Patches`
- Banks (`.5hkrb`) — patterns. `%AppData%\Stupid Audio\5HKR\Banks`
- Jucer `version=` is the binary, About dialog, git tag, and `just build-dist` name.

Editor pages: Main, Mixer, Effects, EQ, Sidechain.

## Lanes

CHH, OHH, Crash, Cym, Shaker, Clave, Cow, Rim.

- Kit per lane: 707 / 808 / 909 / Linn / Sample (default 808). Mix kits across lanes.
- Drop wav / aiff / flac / ogg on a lane to load a one-shot and switch that lane to Sample. File is summed to mono, trimmed to 4 seconds, stored in the patch.
- CHH chokes OHH
- Per-lane Tune, mute/solo, swing, FX send, probability
- Per-lane mixer: Gain, High/Mid/Low, DJ filter, pan, mute, solo, fader
- Extra stem buses CHH … Rim after pan (no master volume, no FX / post EQ / duck). Main Output is the full mix.

## Sequencer

- 16th-note grid, 16 / 32 / 64 steps, per-lane loop length
- Per-step off / ghost / on / accent, flam / ratchet, per-lane probability
- Pattern/lane copy-paste, grid undo, fill hold (pattern 12 / MIDI C3)
- 3 banks × 12 patterns; hold C1 (Ableton C0) for bank 1 pattern 1
- VST3 follows host BPM; standalone uses Tempo

## FX and duck

- Per-lane FX send into a global chorus / delay / flanger / phaser / reverb / compressor / distortion / insert-EQ / filter chain. Off until enabled. Drag to reorder.
- Post-effects 5-band parametric EQ
- Sidechain duck from the plugin audio input (off until enabled)

## Do not

- Sample library, streaming, or multi-sample instruments
- MIDI out of the sequence
- CMake (Projucer only)
- Allocations, locks, or system calls in `processBlock`
- Commercial plugin code

## Stack

- JUCE, Projucer workflow only
- C++17, APVTS for patch knobs
- Just + justfile

## UI

Magenta / cyan chassis. Do not invent another palette.

- `#0f151e` ink
- `#1a2332` chassis
- `#ff00ff` magenta
- `#00ffff` cyan
- `#c8d6e5` metal

`assets/logo.png` is the Affinity export. Source of truth. Never overwrite. `just icon` / `just docs-logo` derive the rest.

## Layout

```
src/
  PluginProcessor.h/.cpp
  PluginEditor.h/.cpp
  dsp/Sequencer.h Voices.h Meter.h FxChain.h EqBiquad.h ParametricEq.h EqAnalyser.h Mixer.h
  ui/VersionCompare.h EffectsPage.h EqPage.h MixerPage.h
  parameters/ParameterIDs.h ParameterLayout.h Library.h
justfile
5HKR.jucer
```

README stays small. LICENSE is MIT. `docs/` is the gh-pages site.
