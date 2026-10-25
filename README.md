<img src="assets/logo.png" alt="5HKR" width="160">

# 5HKR

**5HKR** (Shaker) is a perc groove box: eight synth lanes on a 16th-note grid, pattern banks, per-lane swing and loop length, per-lane sound edit (waveform + kit offsets, or one dropped one-shot sample), a mixer (gain / 3-band / DJ filter / pan / mute / solo / fader), a send into delay/reverb/chorus/flanger/phaser, post EQ, and a sidechain duck. Hosts that support multiple outputs can pull each lane as its own stereo bus (post mixer strip); FX, post EQ, and duck stay on the main mix. Hold MIDI C1 for bank 1 pattern 1 (Ableton C0), C2 for bank 2 pattern 1, like TEW03.

36 factory grooves: bank 1 house/techno, bank 2 latin/afro, bank 3 broken/polyrhythmic. Edit them in `factoryPats` in [src/dsp/Sequencer.h](src/dsp/Sequencer.h).

VST3 and standalone. Stupid Systems LLC. [MIT](LICENSE). Docs: [iamtew.github.io/5HKR](https://iamtew.github.io/5HKR/).

## Release

Windows, Linux, and macOS files come from GitHub Actions (`.github/workflows/dist.yml`). Run it from the Actions tab, or push a numeric tag such as `0.0.1`. The workflow does not publish a release. `<version>` is the `version` in `5HKR.jucer`.

The `dist` artifact holds:

- `5HKR-<version>-win-x64-vst3.zip`
- `5HKR-<version>-win-x64-standalone.zip`
- `5HKR-<version>-win-x64.msi`
- `5HKR-<version>-linux-x64-vst3.zip`
- `5HKR-<version>-linux-x64-standalone.zip`
- `5HKR-<version>-mac-universal-vst3.zip`
- `5HKR-<version>-mac-universal-standalone.zip`

Download that artifact into `dist/` and upload the files on a GitHub release:

```
gh run download <run-id> -n dist -D dist
```

The MSI installs the VST3, the standalone app, or both. Defaults are `Common Files\VST3` and `Program Files\Stupid Systems LLC\5HKR`; each folder can be changed. The standalone install adds a Start menu shortcut.

The Mac zips are ad-hoc signed, not notarized. Gatekeeper blocks them until you right-click Open, or run `xattr -dr com.apple.quarantine` on the `.vst3` and the `.app`. The Linux build uses the system ALSA, FreeType, fontconfig, and X11 libraries.

`just build-dist` on Windows still writes the three Windows files only. Building the MSI needs the .NET 8 SDK. Do not run the `dist` MSI from this OneDrive tree (msiexec 1620). `just build-msi` copies it to `%LOCALAPPDATA%\5HKR\` and selects that file in Explorer. After it is installed, later `dist` double-clicks use the Windows cache like TEW03/K1X/V4JB.
