# Architecture

PluginProcessor owns APVTS, Sequencer, Voices, Ducker, and atomics for every bank slot.

Audio thread:

1. MIDI note-on in C1–range selects a slot; the groove runs while that key is held
2. Sequencer::advance emits Hits (lane, accent, sample offset) using host BPM in VST3, else seqTempo
3. Voices::trigger then Voices::process mix the eight synth hits
4. Volume, then optional Ducker from the input bus, then OutputMeter

Message thread:

- Editor reads slot atomics for the grid and playhead
- Patch XML is APVTS state without bank data; bank XML is a property on that tree plus `.5hkrb` files
- Clicks write steps / loop / length back through the processor (no alloc on the audio thread)
