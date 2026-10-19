#include "dsp/EqBiquad.h"
#include "dsp/Sequencer.h"
#include "dsp/Voices.h"
#include "parameters/Library.h"
#include "parameters/ParameterIDs.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <tuple>

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

    SlotData grow;
    setStep (grow, chh, 0, 2);
    setStep (grow, chh, 4, 1);
    grow.len = 16;
    setPatternLen (grow, 32);
    assert (grow.len == 32);
    assert (getStep (grow, chh, 16) == 2);
    assert (getStep (grow, chh, 20) == 1);
    setPatternLen (grow, 64);
    assert (grow.len == 64);
    assert (getStep (grow, chh, 48) == 2);
    grow.loop[chh] = 5;
    setPatternLen (grow, 16);
    assert (grow.len == 16);
    assert (grow.loop[chh] == 5);

    SlotData wipe;
    wipe.len = 16;
    wipe.loop[chh] = 7;
    setStep (wipe, chh, 0, 2);
    setStep (wipe, chh, 15, 1);
    setStep (wipe, chh, 16, 2); // leftover past len
    clearPatternSteps (wipe);
    assert (getStep (wipe, chh, 0) == 0);
    assert (getStep (wipe, chh, 15) == 0);
    assert (getStep (wipe, chh, 16) == 2);
    assert (wipe.loop[chh] == 7);
    assert (wipe.len == 16);

    SlotData rndA, rndB;
    rndA.len = 16;
    rndB.len = 16;
    randomizePatternSteps (rndA, 1u);
    randomizePatternSteps (rndB, 2u);
    int nOn = 0;
    bool rndSame = true;
    for (int lane = 0; lane < numLanes; ++lane)
        for (int i = 0; i < 16; ++i)
        {
            if (getStep (rndA, lane, i) > 0)
                ++nOn;
            if (getStep (rndA, lane, i) != getStep (rndB, lane, i))
                rndSame = false;
        }
    assert (nOn > 0);
    assert (! rndSame);

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

    // Factory grids are 16 steps of . x X, and no two slots share a groove.
    {
        constexpr int nSlots = Sequencer::numBanks * Sequencer::patternsPerBank;
        SlotData fac[nSlots];
        for (int k = 0; k < nSlots; ++k)
        {
            const auto& f = factoryPats[k / Sequencer::patternsPerBank][k % Sequencer::patternsPerBank];
            for (int lane = 0; lane < numLanes; ++lane)
            {
                assert (std::strlen (f.grid[lane]) == 16);
                assert (std::strspn (f.grid[lane], ".xX") == 16);
                assert (f.loop[lane] <= 16);
            }
            fillFactory (fac[k], k / Sequencer::patternsPerBank, k % Sequencer::patternsPerBank);
        }
        for (int a = 0; a < nSlots; ++a)
            for (int c = a + 1; c < nSlots; ++c)
            {
                const bool same = std::memcmp (fac[a].words, fac[c].words, sizeof (fac[a].words)) == 0
                                  && std::memcmp (fac[a].loop, fac[c].loop, sizeof (fac[a].loop)) == 0;
                if (same)
                    std::printf ("factory slot %d == slot %d\n", a, c);
                assert (! same);
            }
    }

    {
        ScFilter f;
        f.prepare (44100.0);
        float loRms = 0.f, hiRms = 0.f;
        const float wLo = 2.f * 3.14159265f * 100.f / 44100.f;
        const float wHi = 2.f * 3.14159265f * 8000.f / 44100.f;
        for (int i = 0; i < 44100; ++i)
        {
            const float lo = f.process (std::sin (wLo * (float) i), 20.f, 400.f);
            loRms += lo * lo;
        }
        f.prepare (44100.0);
        for (int i = 0; i < 44100; ++i)
        {
            const float hi = f.process (std::sin (wHi * (float) i), 20.f, 400.f);
            hiRms += hi * hi;
        }
        loRms = std::sqrt (loRms / 44100.f);
        hiRms = std::sqrt (hiRms / 44100.f);
        assert (loRms > 0.4f);
        assert (hiRms < 0.05f);

        f.prepare (44100.0);
        loRms = hiRms = 0.f;
        for (int i = 0; i < 44100; ++i)
        {
            const float lo = f.process (std::sin (wLo * (float) i), 2000.f, 20000.f);
            loRms += lo * lo;
        }
        f.prepare (44100.0);
        for (int i = 0; i < 44100; ++i)
        {
            const float hi = f.process (std::sin (wHi * (float) i), 2000.f, 20000.f);
            hiRms += hi * hi;
        }
        loRms = std::sqrt (loRms / 44100.f);
        hiRms = std::sqrt (hiRms / 44100.f);
        assert (hiRms > 0.4f);
        assert (loRms < 0.05f);
    }

    {
        Voices v;
        float zeroTune[numLanes] {};
        float lanes[numLanes] {};
        auto energy = [&] (int k, int lane, int n, int skip)
        {
            v.prepare (44100.0);
            int kits[numLanes];
            for (int i = 0; i < numLanes; ++i)
                kits[i] = k;
            v.setKit (kits);
            v.setTune (zeroTune);
            v.trigger (lane, true);
            float e = 0.f, peak = 0.f, late = 0.f;
            for (int i = 0; i < n; ++i)
            {
                v.process (lanes);
                const float x = lanes[lane];
                const float a = x * x;
                e += a;
                peak = std::max (peak, std::abs (x));
                if (i >= skip)
                    late += a;
            }
            return std::tuple<float, float, float> { e, peak, late };
        };
        const auto [e808chh, p808chh, late808] = energy (kit808, chh, 800, 150);
        const auto [e808cow, p808cow, lateCow] = energy (kit808, cowbell, 800, 150);
        const auto [e909chh, p909chh, late909] = energy (kit909, chh, 800, 150);
        assert (p808chh > 0.2f);
        assert (p808cow > 0.2f);
        assert (std::abs (e808chh - e808cow) / (e808chh + e808cow + 1e-9f) > 0.1f);
        assert (late808 > late909 * 1.5f);
        (void) p909chh;
        (void) lateCow;
        (void) e909chh;
    }

    {
        int in[] = { 4, 0, 2 };
        int out[hkr::fxCount] {};
        const auto packed = hkr::packFxOrder (in, 3);
        assert (hkr::unpackFxOrder (packed, out) == 3);
        assert (out[0] == 4 && out[1] == 0 && out[2] == 2);
        assert (hkr::completeFxOrder (out, 3) == hkr::fxCount);
        assert (out[3] == 1 && out[4] == 3);

        const auto peak = hkr::eqBiquad (hkr::eqTypePeak, 44100.f, 1000.f, 0.7f, 6.f);
        const float w = 2.f * 3.14159265f * 1000.f / 44100.f;
        const float mag = hkr::eqBiquadMag (peak, w);
        assert (mag > 1.8f && mag < 2.2f);
    }

    std::puts ("groove_check ok");
    return 0;
}
