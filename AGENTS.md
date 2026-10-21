# 5HKR – Cursor Rules

Percussion groove box: 8 synth lanes, step sequencer, pattern banks, sidechain duck.
A lane can play one loaded one-shot instead of its synth. Not a sample library.

## Stack
- JUCE (Projucer workflow only – no CMake)
- C++17, real-time safe: no allocations, no locks, no system calls in processBlock
- APVTS for patch knobs
- Just + justfile

## Layout
src/
  PluginProcessor.h/.cpp
  PluginEditor.h/.cpp
  dsp/Sequencer.h Voices.h Meter.h FxChain.h EqBiquad.h ParametricEq.h EqAnalyser.h Mixer.h
  ui/EffectsPage.h EqPage.h MixerPage.h
  parameters/ParameterIDs.h ParameterLayout.h Library.h
justfile
5HKR.jucer

## Version
- Jucer `version=` is what the binary and About dialog show.
- `just build-dist` names the zips and MSI from the jucer version.

## Do NOT
- Introduce CMake
- Allocate on the audio thread
- Add a sample library, streaming, or multi-sample instruments
- Copy commercial plugin code
