# Architecture

PluginProcessor owns APVTS, Sequencer, Voices, FxChain, post EQ, EqAnalyser, Ducker, ScFilter (L/R), and atomics for every bank slot.

Audio thread:

1. MIDI note-on in C1–range selects a slot; the groove runs while that key is held
2. Sequencer::advance emits Hits (lane, accent, sample offset) using host BPM in VST3, else seqTempo
3. Voices::trigger then Voices::process fill eight lane samples
4. Per-lane FX send splits dry vs send bus; send goes through the global FxChain (chorus, delay, flanger, phaser, reverb)
5. Dry + FX, then 5-band post EQ, then optional HP/LP on the input bus into Ducker (full mix including FX tails). Listen replaces the output with that filtered key, then EqAnalyser + OutputMeter

Message thread:

- Editor reads slot atomics for the grid and playhead
- Patch XML is APVTS state without bank data; bank XML is a property on that tree plus `.5hkrb` files
- Clicks write steps / loop / length back through the processor (no alloc on the audio thread)
