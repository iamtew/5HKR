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

// Grow 16→32/64 by tiling the current length so extra bars are not empty.
inline void setPatternLen (SlotData& s, int newLen)
{
    const int old = clampLen ((int) s.len);
    newLen = clampLen (newLen);
    if (newLen > old)
        for (int lane = 0; lane < numLanes; ++lane)
            for (int i = old; i < newLen; ++i)
                setStep (s, lane, i, getStep (s, lane, i % old));
    for (int lane = 0; lane < numLanes; ++lane)
    {
        int lp = std::max (1, (int) s.loop[lane]);
        if (lp == old || lp > newLen)
            lp = newLen;
        s.loop[lane] = (uint8_t) lp;
    }
    s.len = (uint8_t) newLen;
}

// Zero steps in the audible length. Loop ends and leftover bars past len stay.
inline void clearPatternSteps (SlotData& s)
{
    const int n = clampLen ((int) s.len);
    for (int lane = 0; lane < numLanes; ++lane)
        for (int i = 0; i < n; ++i)
            setStep (s, lane, i, 0);
}

// ~55% off, ~35% on, ~10% accent. LCG so tests can pin a seed.
inline void randomizePatternSteps (SlotData& s, uint32_t seed)
{
    const int n = clampLen ((int) s.len);
    uint32_t x = seed ? seed : 1u;
    for (int lane = 0; lane < numLanes; ++lane)
        for (int i = 0; i < n; ++i)
        {
            x = x * 1664525u + 1013904223u;
            const int r = (int) (x % 100u);
            setStep (s, lane, i, r < 10 ? 2 : (r < 45 ? 1 : 0));
        }
}

// Factory grooves, one per slot. Edit the grids directly: each string is
// exactly 16 steps, '.' off, 'x' on, 'X' accent. Lane order is
// CHH, OHH, Crash, Cym, Shaker, Clave, Cow, Rim.
// loop = per-lane loop length in the same lane order, 0 = full 16.
// With a short loop only the first `loop` steps of that grid play.
struct FactoryPat
{
    const char* grid[numLanes];
    uint8_t loop[numLanes];
};

inline constexpr FactoryPat factoryPats[3][12] = {
    // Bank 1: house / techno, straight.
    //     1   2   3   4
    {
        // 1 Classic offbeat
        { { "X.x.X.x.X.x.X.x.", "..x...x...x...x.", "X...............", "................",
            ".x.x.x.x.x.x.x.x", "................", "................", "....x.......x..." } },
        // 2 Minimal
        { { "..x...x...x...x.", "................", "................", "................",
            "x...x...x...x...", "................", "................", "....X.......X..." } },
        // 3 Rolling 16ths
        { { "XxxxXxxxXxxxXxxx", "..x...x...x...x.", "................", "................",
            "................", "................", "................", "....x..x....x..." } },
        // 4 Shaker led
        { { "................", "..x...x...x...x.", "................", "................",
            "Xx.xXx.xXx.xXx.x", "................", "................", "....x.......x..." } },
        // 5 Tribal techno
        { { "x.x.x.x.x.x.x.x.", "................", "................", "................",
            ".xx..xx..xx..xx.", "................", "X..x..x...X..x..", "..x..x....x..x.." } },
        // 6 Breakdown, no hats
        { { "................", "................", "X...............", "x.......x.......",
            "........x.x.xxxx", "................", "................", "............x.x." } },
        // 7 Peak time
        { { "XxXxXxXxXxXxXxXx", "..X...X...X...X.", "X...............", "........x.......",
            ".x.x.x.x.x.x.x.x", "................", "....x.......x...", "....X.......X..x" } },
        // 8 Garage shuffle
        { { "x.xxx.x.x.xxx.x.", "......x.......x.", "................", "................",
            "..x...x...x...x.", "................", "................", "....X..x....X..." } },
        // 9 Ride techno
        { { "................", "................", "................", "x.X.x.X.x.X.x.X.",
            "xxxxxxxxxxxxxxxx", "................", "................", "....x.......x..." } },
        // 10 Acid cowbell
        { { "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "................", "................",
            "................", "................", "X.xx..x.X..x.x..", "....x.......x..." } },
        // 11 Deep house
        { { "x.x.x.x.x.x.x.xx", "..x.......x.....", "................", "................",
            ".xx..xx..xx..xx.", "...x......x.....", "................", "....x.......x..." } },
        // 12 Build fill
        { { "x.x.x.x.xxxxXXXX", "..x...x.........", "X..............X", "................",
            "x.x.x.x.xxxxxxxx", "................", "................", "....x...x.x.XXXX" } },
    },
    // Bank 2: latin / afro. Add lane swing to taste.
    //     1   2   3   4
    {
        // 1 Son clave 3-2
        { { "x.x.x.x.x.x.x.x.", "................", "................", "................",
            "XxxxXxxxXxxxXxxx", "X..x..x...x.x...", "X...x.x.X...x.x.", "................" } },
        // 2 Son clave 2-3
        { { "................", "................", "................", "x.xx.x.xx.x.x.x.",
            "x.xxx.xxx.xxx.xx", "..x.x...X..x..x.", "................", "...x.......x...." } },
        // 3 Rumba clave
        { { "................", "................", "................", "................",
            "XxxxXxxxXxxxXxxx", "X..x...x..x.x...", "................", "......x.x.....x." } },
        // 4 Cascara
        { { "................", "................", "................", "X.xX.xx.X.xX.xx.",
            "................", "X..x..x...x.x...", "................", "..x...x...x...x." } },
        // 5 Mambo bell
        { { "................", "................", "X...............", "................",
            "x.x.x.x.x.x.x.x.", "..x.x...X..x..x.", "X.x.x.xxX.xxx.x.", "......x.......x." } },
        // 6 Bembe, 12-step bell against 16
        { { "................", "................", "................", "................",
            "x..x..x..x..x..x", "................", "X.x.xx.x.x.x....", "......x.......x." },
          { 0, 0, 0, 0, 0, 0, 12, 0 } },
        // 7 Samba
        { { "................", "................", "................", "................",
            "XxxXXxxXXxxXXxxX", "................", "X.x..x.xX.x..x.x", "x.xx.xx.x.xx.xx." } },
        // 8 Baiao
        { { "................", "................", "................", "................",
            "XxxxXxxxXxxxXxxx", "................", "..x...x...x...x.", "X..x....X..x...." } },
        // 9 Afrobeat
        { { "x.xxx.x.x.xxx.x.", "......x.......x.", "................", "................",
            "................", "................", "X.x..x.x..x.x...", "....X.......X..x" } },
        // 10 Songo
        { { "................", "................", "................", "X.x.X.x.X.x.X.x.",
            "x.x.x.x.x.x.x.x.", "..x.x...X..x..x.", "................", "...x..x....x..x." } },
        // 11 Guaguanco
        { { "................", "................", "................", "................",
            "x..xx..xx..xx..x", "X..x...x..x.x...", "................", "..x...xx..x...x." } },
        // 12 Comparsa fill
        { { "................", "..............X.", "X...............", "................",
            "XxxxXxxxXxxxXXXX", "X..x..x...x.x...", "X.x.X.x.X.xxX.XX", "........x.x.xxXX" } },
    },
    // Bank 3: broken / polyrhythmic. Short lane loops drift against the bar.
    //     1   2   3   4
    {
        // 1 Five against four
        { { "x.x.x.x.x.x.x.x.", "................", "................", "................",
            "................", "X..x............", "................", "....x.......x..." },
          { 0, 0, 0, 0, 0, 5, 0, 0 } },
        // 2 Three and seven
        { { "................", "................", "................", "................",
            "X.x.............", "................", "X..x.x..........", "....x.......x..." },
          { 0, 0, 0, 0, 3, 0, 7, 0 } },
        // 3 Seven drift
        { { "X.xx.x..........", "......x.......x.", "................", "................",
            "................", "................", "................", "...x......x....." },
          { 7, 0, 0, 0, 0, 0, 0, 0 } },
        // 4 Broken hats
        { { "x.xx..x.x..xx.x.", "...x.......x....", "................", "................",
            ".x...x.x...x...x", "................", "................", "....x..x.x..x..." },
          { 0, 0, 0, 0, 9, 0, 0, 0 } },
        // 5 Six and nine
        { { ".x.x.x.x.x.x.x.x", "................", "................", "x...x..x........",
            "Xx.xx...........", "................", "................", "....x.......x..." },
          { 0, 0, 0, 9, 6, 0, 0, 0 } },
        // 6 Sparse ghosts
        { { "x.....x.......x.", "................", ".......x........", "................",
            "................", "X.........x.....", "................", "..x.x..x.x......" },
          { 0, 0, 0, 0, 0, 0, 0, 11 } },
        // 7 Eleven and thirteen
        { { "x.xxx.xxx.xxx.xx", "................", "................", "................",
            "................", "X..x..x.x.......", "X.....x.....x...", "....x.......x..." },
          { 0, 0, 0, 0, 0, 11, 13, 0 } },
        // 8 Thirteen hats
        { { "Xx.x.xx.x.x.x...", "......x.......x.", "................", "................",
            "x.x.x.x.x.x.x.x.", "................", "................", "....X.......X..." },
          { 13, 0, 0, 0, 0, 0, 0, 0 } },
        // 9 Displaced
        { { ".x.x.x.x.x.x.x.x", "x...............", "...X............", "................",
            "................", ".....x.....x....", "................", "...x...x..x....x" } },
        // 10 Polymeter stack
        { { "X.x.............", "................", "................", "................",
            "X.x.x...........", "X..x..x.........", "x...x...........", "....x.......x..." },
          { 3, 0, 0, 0, 5, 7, 9, 0 } },
        // 11 Glitch
        { { "xx..x.xxx...x.xx", ".......x........", "................", "..x..x..........",
            "................", "................", "................", "X..x.xX...x..xX." },
          { 0, 0, 0, 6, 0, 0, 0, 0 } },
        // 12 Chaos fill
        { { "X..x.x..xxx.XxXx", "...........x....", "X..............X", "x.x.x.x.........",
            "XxXxx...........", "................", "x...............", "........x.xxXXXX" },
          { 0, 0, 0, 7, 5, 0, 3, 0 } },
    },
};

inline void fillFactory (SlotData& s, int bank, int pat)
{
    s = SlotData();
    s.len = 16;
    const auto& f = factoryPats[std::clamp (bank, 0, 2)][std::clamp (pat, 0, 11)];
    for (int lane = 0; lane < numLanes; ++lane)
    {
        const char* g = f.grid[lane];
        for (int i = 0; i < 16 && g[i] != '\0'; ++i)
            setStep (s, lane, i, g[i] == 'X' ? 2 : (g[i] == 'x' ? 1 : 0));
        if (f.loop[lane] != 0)
            s.loop[lane] = f.loop[lane];
    }
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

static_assert (sizeof (factoryPats) / sizeof (factoryPats[0]) == Sequencer::numBanks
                   && sizeof (factoryPats[0]) / sizeof (factoryPats[0][0]) == Sequencer::patternsPerBank,
               "factoryPats needs one groove per bank/pattern slot");

} // namespace hkr
