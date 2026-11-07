# Grid

This is the Main page: eight lanes, a step grid, and the knobs that make a pattern feel like a groove instead of a metronome having a panic.

Lanes, left to right in spirit: **CHH**, **OHH**, **Crash**, **Cym**, **Shaker**, **Clave**, **Cow**, **Rim**. **M** and **S** on the right mute and solo that voice, same as the mixer. Closed hat chokes open hat, because 1982 asked nicely.

The kit menu under the name picks **707 / 808 / 909 / Linn / Sample** for that lane only (default 808). Mix kits across lanes. Drop a wav, aiff, flac, or ogg on a lane to load it and switch that lane to Sample. The file is summed to mono, trimmed to four seconds, and saved in the patch.

## Cells

Click a cell: off → ghost (dim cyan) → on (cyan) → accent (magenta). On, ghost, and accent also preview the lane, so you can hear what you just committed.

Shift-click a cell to cycle flam (the little dot) / ratchet ×2 / ratchet ×4. Empty cells stay empty. Right-click a cell to set that lane’s loop end (magenta outline). Click the same end again to restore full length.

Right-click a lane name for Copy lane, Paste lane, Random lane, or Euclidean. Euclidean fills the loop; an empty lane uses half the steps, which is usually enough mischief.

## Lane knobs

- **Tune** ±12 st
- **Swing** 50–75%
- **FX** 0–100% into the effects chain
- **Prob** 0–100% chance each step fires (default 100, because surprise is optional)

## Edit sound

**Edit** on a lane opens that sound. Lane names, kit menus, and Edit buttons stay on the left. Edit on another lane switches sound; Edit on the same lane goes back to the grid. The edited lane name becomes **Play** (preview that sound; mute stays as it was).

Synth kits: Tune / Decay / Click / Drive / HPF / Tone / Level offset the kit spec. 50% is the stock sound, the one the machine would have shipped if it had shipped.

Sample: Tune / Start / End / Attack / Release / Level / Reverse, plus **Load** / **Unload** (or drop a file on the waveform). Attack and Release are fades over the cropped region; they draw as a faded overlay on the waveform. Unload drops the file; pick a synth kit to leave Sample. A magenta line on the waveform follows the hit while it plays.

The mixer strip for that lane sits on the right while you edit (mute/solo live there; the grid M/S are hidden so you do not have two mutes to argue with).

## Pattern

**Bank** / **Pat** — 3 banks × 12 patterns. **1 / 2 / 4 bar** is 16 / 32 / 64 sixteenths. Extra bars tile the current length. Use the step pager when the pattern is longer than one bar.

**Random** / **Clear** fill or wipe every step of the current pattern. Loops, length, and knobs stay. **Copy** / **Paste** the whole current pattern. Ctrl+C / Ctrl+V. Ctrl+Z undoes the last grid edit (one level, it toggles).

**Fill** — hold to play pattern 12 of the current bank without changing the selected pattern. MIDI note 60 (C3) does the same while a pattern key is held (or standalone Play).
