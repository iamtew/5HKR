<img src="assets/logo.png" alt="5HKR" width="160">

# 5HKR

**5HKR** (Shaker) is a perc groove box: eight synth lanes on a 16th-note grid, pattern banks, per-lane swing and loop length, a send into delay/reverb/chorus/flanger/phaser, post EQ, and a sidechain duck. Hold MIDI C1 for bank 1 pattern 1 (Ableton C0), C2 for bank 2 pattern 1, like TEW03.

36 factory grooves: bank 1 house/techno, bank 2 latin/afro, bank 3 broken/polyrhythmic. Edit them in `factoryPats` in [src/dsp/Sequencer.h](src/dsp/Sequencer.h).

VST3 and standalone. Stupid Systems LLC. [MIT](LICENSE). Docs: [iamtew.github.io/5HKR](https://iamtew.github.io/5HKR/).

## Release

`just build-dist` writes `dist/5HKR-<version>-win-x64-vst3.zip`, `dist/5HKR-<version>-win-x64-standalone.zip`, and `dist/5HKR-<version>-win-x64.msi`. Defaults are `Common Files\VST3` and `Program Files\Stupid Systems LLC\5HKR`. Building the MSI needs the .NET 8 SDK.
