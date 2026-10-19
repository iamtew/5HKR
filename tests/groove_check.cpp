#include "dsp/Sequencer.h"
#include "parameters/Library.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    using namespace hkr;

    int bank = -1, pat = -1;
    assert (Sequencer::mapKeyToSlot (24, bank, pat) && bank == 0 && pat == 0);
    assert (Sequencer::mapKeyToSlot (28, bank, pat) && bank == 0 && pat == 4);
    assert (Sequencer::mapKeyToSlot (36, bank, pat) && bank == 1 && pat == 0);
    assert (Sequencer::mapKeyToSlot (59, bank, pat) && bank == 2 && pat == 11);
    assert (! Sequencer::mapKeyToSlot (23, bank, pat));
    assert (! Sequencer::mapKeyToSlot (60, bank, pat));

    SlotData s;
    setStep (s, chh, 0, 1);
    setStep (s, chh, 1, 2);
    setStep (s, chh, 2, 0);
    assert (getStep (s, chh, 0) == 1);
    assert (getStep (s, chh, 1) == 2);
    assert (getStep (s, chh, 2) == 0);
    setStep (s, chh, 63, 2);
    assert (getStep (s, chh, 63) == 2);

    assert (laneStep (16, 5) == 1);
    assert (laneStep (0, 7) == 0);
    assert (laneStep (7, 7) == 0);
    assert (laneStep (8, 16) == 8);

    const double sps = 44100.0 * 60.0 / 120.0 / 4.0;
    assert (std::abs (swingOffset (0.5f, sps)) < 1e-9);
    assert (std::abs (swingOffset (0.75f, sps) - 0.5 * sps) < 1e-6);

    SlotData groove;
    fillFactory (groove, 0, 0);
    assert (groove.len == 16);
    assert (getStep (groove, chh, 0) == 2);

    Sequencer seq;
    seq.prepare (44100.0);
    float swing[numLanes];
    for (int i = 0; i < numLanes; ++i)
        swing[i] = 0.5f;
    Hit hits[64];
    const int n0 = seq.advance (groove, 1, 120.f, true, swing, 0xff, hits, 64);
    assert (n0 > 0);
    assert (hits[0].offset == 0);
    assert (seq.playhead() >= 0);

    const int stepSamples = (int) std::ceil (sps);
    seq.advance (groove, stepSamples, 120.f, true, swing, 0xff, hits, 64);
    assert (seq.playhead() >= 1);

    const int nOff = seq.advance (groove, 64, 120.f, false, swing, 0xff, hits, 64);
    assert (nOff == 0);
    assert (seq.playhead() == -1);

    SlotData slots[Sequencer::numBanks][Sequencer::patternsPerBank];
    for (int b = 0; b < Sequencer::numBanks; ++b)
        for (int p = 0; p < Sequencer::patternsPerBank; ++p)
            fillFactory (slots[b][p], b, p);
    slots[1][0].loop[clave] = 5;
    setStep (slots[1][0], clave, 4, 2);
    const auto xml = writeBankXml (slots);
    SlotData back[Sequencer::numBanks][Sequencer::patternsPerBank];
    assert (readBankXml (xml, back));
    assert (back[1][0].loop[clave] == 5);
    assert (getStep (back[1][0], clave, 4) == 2);
    assert (getStep (back[0][0], chh, 0) == getStep (slots[0][0], chh, 0));

    std::puts ("groove_check ok");
    return 0;
}
