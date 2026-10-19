#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace hkr
{

enum Lane : int
{
    chh = 0,
    ohh,
    crash,
    cymbal,
    shaker,
    clave,
    cowbell,
    rim,
    numLanes
};

inline constexpr const char* laneNames[numLanes] = {
    "CHH", "OHH", "Crash", "Cym", "Shaker", "Clave", "Cow", "Rim"
};

// 2 bits per step: 0 off, 1 on, 2 accent. 16 steps per uint32.
struct SlotData
{
    static constexpr int maxSteps = 64;
    static constexpr int wordsPerLane = maxSteps / 16;

    uint8_t len = 16; // 16 / 32 / 64 sixteenths
    uint8_t loop[numLanes] {};
    uint32_t words[numLanes][wordsPerLane] {};

    SlotData()
    {
        for (int i = 0; i < numLanes; ++i)
            loop[i] = 16;
    }
};

inline int clampLen (int v)
{
    if (v <= 16)
        return 16;
    if (v <= 32)
        return 32;
    return SlotData::maxSteps;
}

inline int laneStep (int patStep, int loopLen)
{
    loopLen = std::max (1, loopLen);
    return ((patStep % loopLen) + loopLen) % loopLen;
}

// swing 0.5 = straight. 0.75 = odd 16ths delayed by half a step.
inline double swingOffset (float swing, double samplesPerStep)
{
    const float s = std::clamp (swing, 0.5f, 0.75f);
    return (double) (s - 0.5f) * 2.0 * samplesPerStep;
}

inline int getStep (const SlotData& s, int lane, int i)
{
    if (lane < 0 || lane >= numLanes || i < 0 || i >= SlotData::maxSteps)
        return 0;
    const int w = i / 16;
    const int sh = (i % 16) * 2;
    return (int) ((s.words[lane][w] >> sh) & 3u);
}

inline void setStep (SlotData& s, int lane, int i, int v)
{
    if (lane < 0 || lane >= numLanes || i < 0 || i >= SlotData::maxSteps)
        return;
    const int w = i / 16;
    const int sh = (i % 16) * 2;
    const uint32_t mask = 3u << sh;
    s.words[lane][w] = (s.words[lane][w] & ~mask) | (((uint32_t) (v & 3)) << sh);
}

inline void fillFactory (SlotData& s, int bank, int pat)
{
    s = SlotData();
    s.len = 16;
    const int motif = (bank * 12 + pat) % 8;

    auto on = [&] (int lane, int i, int v = 1)
    {
        if (i >= 0 && i < 16)
            setStep (s, lane, i, v);
    };

    // Closed hats on the 16ths, a few accents.
    for (int i = 0; i < 16; ++i)
        on (chh, i, (i % 4 == 0) ? 2 : 1);
    on (ohh, 14, 1);
    if (motif != 0)
        on (crash, 0, 1);
    on (cymbal, 8, 1);
    for (int i = 0; i < 16; i += 2)
        on (shaker, i, 1);
    on (clave, 0, 2);
    on (clave, 3, 1);
    on (clave, 6, 1);
    on (clave, 10, 1);
    on (cowbell, 4, 1);
    on (cowbell, 12, 1);
    on (rim, 4, 1);
    on (rim, 7, 1);
    on (rim, 13, 1);

    if (motif == 1)
        s.loop[clave] = 5; // 5-step clave against 16
    if (motif == 2)
        s.loop[shaker] = 6;
    if (motif == 3)
        s.loop[rim] = 7;
}

struct Hit
{
    int offset = 0;
    int lane = 0;
    bool accent = false;
};

struct Sequencer
{
    static constexpr int numBanks = 3;
    static constexpr int patternsPerBank = 12;
    static constexpr int keyMapBase = 24; // C1 / Ableton C0 = bank 1 pattern 1

    static bool mapKeyToSlot (int note, int& bank, int& pat)
    {
        const int idx = note - keyMapBase;
        if (idx < 0 || idx >= numBanks * patternsPerBank)
            return false;
        bank = idx / patternsPerBank;
        pat = idx % patternsPerBank;
        return true;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        phase = 0.0;
        head = -1;
        wasRun = false;
    }

    int playhead() const { return head; }

    int advance (const SlotData& slot, int nSamples, float bpm, bool running,
                 const float swing[numLanes], uint8_t enabled,
                 Hit* hits, int maxHits)
    {
        if (! running)
        {
            wasRun = false;
            phase = 0.0;
            head = -1;
            return 0;
        }

        bpm = std::max (20.f, bpm);
        const int len = clampLen ((int) slot.len);
        const double sps = sr * 60.0 / (double) bpm / 4.0;
        if (sps < 1.0 || nSamples <= 0 || hits == nullptr || maxHits <= 0)
            return 0;

        if (! wasRun)
        {
            wasRun = true;
            phase = 0.0;
        }

        const double start = phase;
        const double end = phase + (double) nSamples;
        const double loopSamps = (double) len * sps;
        int nHits = 0;

        for (int st = 0; st < len; ++st)
        {
            for (int lane = 0; lane < numLanes; ++lane)
            {
                if ((enabled & (uint8_t) (1u << lane)) == 0)
                    continue;
                const int ls = laneStep (st, std::min (len, std::max (1, (int) slot.loop[lane])));
                const int v = getStep (slot, lane, ls);
                if (v == 0)
                    continue;

                double t = (double) st * sps;
                if ((ls & 1) != 0)
                    t += swingOffset (swing[lane], sps);

                for (int k = -1; k <= 1; ++k)
                {
                    const double tt = t + (double) k * loopSamps;
                    if (tt < start || tt >= end)
                        continue;
                    if (nHits >= maxHits)
                        return nHits;
                    hits[nHits++] = { (int) (tt - start), lane, v == 2 };
                }
            }
        }

        phase = std::fmod (end, loopSamps);
        if (phase < 0.0)
            phase += loopSamps;
        head = std::min (len - 1, (int) (phase / sps));
        return nHits;
    }

private:
    double sr = 44100.0;
    double phase = 0.0;
    int head = -1;
    bool wasRun = false;
};

} // namespace hkr
