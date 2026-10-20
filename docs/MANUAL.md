# 5HKR manual

Perc groove box. Eight synth lanes, not a sample player. The groove runs while a mapped MIDI key is held.

## Pages

**Main / Effects / EQ / Sidechain** (title bar) — chevrons or click the name to switch.

Click **5HKR** + version in the top left for About.

**Volume** (title bar, right) — master fader and a peak meter. Clip lights if the output hits 0 dBFS.

## Grid (Main)

Lanes: CHH, OHH, Crash, Cym, Shaker, Clave, Cow, Rim. The lane name mutes that voice. The kit menu under the name picks 707 / 808 / 909 / Linn for that lane only (default 808). Mix kits across lanes. Analog models, not a sample ROM.

- Click a cell: off → on (cyan) → accent (magenta). On and accent also preview the lane.
- Right-click a cell to set that lane’s loop end (magenta outline). Click the same end again to restore full length.
- **Tune** ±12 st, **Swing** 50–75%, and **FX** 0–100% per lane (how much of that lane goes into the effects chain).
- **Edit** on a lane opens that sound: left column picks the lane, waveform plus Kit / Tune / Decay / Click / Drive / HPF / Tone / Level offset the kit spec (50% is the stock sound). A magenta line on the waveform follows the hit while it plays. **Grid** returns to the sequencer.
- **Bank** / **Pat** — 3 banks × 12 patterns.
- **1 / 2 / 4 bar** — 16 / 32 / 64 sixteenths. Extra bars tile the current length. Use the step pager when longer than one bar.
- **Random** / **Clear** — fill or wipe every step of the current pattern. Loops, length, and knobs stay.

## Factory banks

Every slot starts with its own one-bar groove (**Init Bank** restores them). Pattern 12 in each bank is a fill.

- **Bank 1** — house / techno, straight: offbeat hats, rolling 16ths, shaker-led, tribal, breakdown, peak time.
- **Bank 2** — latin / afro: son and rumba clave, cascara, mambo bell, bembe, samba, baiao, afrobeat, songo. Add lane swing to taste.
- **Bank 3** — broken / polyrhythmic: short lane loops (3, 5, 6, 7, 9, 11, 13 steps) that drift against the bar, displaced hats, ghost rims.

Saved sessions and `.5hkrb` banks keep their own patterns.

## MIDI

Hold C1 for bank 1 pattern 1 (Ableton C0). C2 is bank 2 pattern 1. Mapping starts at MIDI note 24; 36 slots. VST3 follows host BPM; standalone uses the Tempo parameter (120 unless automated).

## Library

**Init Patch** / **Init Bank** in the title bar: chevrons step the list; click for Init / Save / Save As / Export / Import.

- Patches (`.5hkrp`) — knobs. `%AppData%\Stupid Systems LLC\5HKR\Patches`
- Banks (`.5hkrb`) — patterns. `%AppData%\Stupid Systems LLC\5HKR\Banks`

## Sidechain

Ducks the mixed lanes from the plugin audio input. **Sidechain** is off until you turn it on. **HP** / **LP** isolate the part of the input the ducker follows. **Listen** puts that filtered key on the output so you can hear it (standalone can feed back if a live mic is the input). Then Thresh, Amount, Attack, Release, Mix. The plot under the knobs is that filtered key (cyan lines mark HP / LP).

## Effects

Click a name in the left rail to turn it on. Drag rails to reorder. Enabled effects show knobs: Chorus, Delay (sync or free), Flanger, Phaser, Reverb. Each has its own Mix. Lane **FX** must be up or the chain is silent.

## EQ

Post-effects 5-band EQ (K1X-style). Toggle EQ, drag nodes for freq/gain, scroll for Q, inspector on the left.

Drag the window corner to scale; the scale is saved with the session.
