# Architecture

PluginProcessor owns APVTS, Sequencer, Voices, LaneMixer, FxChain, post EQ, EqAnalyser (output + sidechain key), Ducker, ScFilter (L/R), and atomics for every bank slot.

Audio thread:

1. MIDI note-on in C1–range selects a slot; the groove runs while that key is held
2. Sequencer::advance emits Hits (lane, accent, sample offset) using host BPM in VST3, else seqTempo
3. Voices::trigger then Voices::process fill eight lane samples (`laneKit*` picks 707 / 808 / 909 / Linn, or Sample)
4. LaneMixer: per-lane trim, 3-band EQ, DJ filter, fader, solo mask, constant-power pan (stereo)
5. Per-lane FX send splits that processed lane dry vs send bus; send goes through the global FxChain (chorus, delay, flanger, phaser, reverb)
6. Dry + FX, then 5-band post EQ, then HP/LP on the input bus (always, for the key spectrum) into Ducker when on (full mix including FX tails). Listen replaces the output with that filtered key, then EqAnalyser + OutputMeter
7. Optional extra stereo output buses (one per lane, named from `laneNames`) copy the post-strip panned lane before the send split. Disabled unless the host enables them. FX / post EQ / duck stay on bus 0.

Message thread:

- Editor reads slot atomics for the grid and playhead
- Patch XML is APVTS state without bank data; bank XML is a property on that tree plus `.5hkrb` files. A lane sample is a `LANE_SAMPLES` child on the patch, not the bank
- Clicks write steps / loop / length back through the processor (no alloc on the audio thread)
