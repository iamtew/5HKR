# 5HKR - Shaker, a groove instrument!
*Rhythmic groves that makes you shake your ass!*

## Sibling plugins:
- `~/src/TEW03`
- `~/src/K1X`
- `~/src/V4JB`

## 5HKR shares with siblings:
- Top bar style, with name on top left and icon
- Scalable UI
- Can save presets
- Master volume and indicator in top right corner
- VST3 and Standalone
- Same build procedure using Just, same kinda of targets
- Same artifacts; installer, vst3 zip and standalone zip
- Icon/logo under `./assets` folder

# Instrument functions
Works like a step sequencer groove box with multiple instruments lanes.
Each instrument is a different percussive sound that can be enabled or not.
Sounds like open hihat, closed hihat, crash, cymbal, shakers, clave, cowbell, etc.
Default pattern length is 1 bar, of 16 steps. 
Pattern length can go up to 4 bars. Increments of 1, 2 and 4.
Lanes can be shorter than the full bar, then they should loop at that point and loop out-of-sync with the bars. This will create syncopated rhythms etc because it's not perfect on sync.
When a MIDI note plays, it plays the lenght of the pattern. 
If a MIDI notes plays for 2 bars, a 1 bar pattern will loop, etc.
Instrument lane loop points that are shorter than a bar will loop shorter within the pattern loop that is playing.
Implement pattern banks like `TEW03` has and play Bank 1 Pattern 1 on C0 and Bank 2 Pattern 1 on C1 etc. like `TEW03` does.
Every lane has a swing-controller, a rotary, to control swing percentage of that lane.
Each step can be off/on/accent like a classic drum machine.
Plugin supports sidechain signal input, to duck the sound based on the sidechain signal.
Full control panel for side channel settings, like attack/release/threshold, etc.

## Colors:
- #1a2332
- #FF00FF
- #00FFFF
- #000000
- #FFFFFF
