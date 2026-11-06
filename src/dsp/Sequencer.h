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

// Last entry is a one-shot sample, not a row in the synth coefficient table.
inline constexpr int numSynthKits = 4;
inline constexpr int numKits = 5;
inline constexpr const char* kitNames[numKits] = { "707", "808", "909", "Linn", "Sample" };
enum Kit : int { kit707 = 0, kit808, kit909, kitLinn, kitSample };

// 2 bits per step: 0 off, 1 on, 2 accent, 3 ghost. 16 steps per uint32.
// Parallel mods: 0 none, 1 flam, 2 ratchet x2, 3 ratchet x4.
struct SlotData
{
    static constexpr int maxSteps = 64;
    static constexpr int wordsPerLane = maxSteps / 16;

    uint8_t len = 16; // 16 / 32 / 64 sixteenths
    uint8_t loop[numLanes] {};
    uint32_t words[numLanes][wordsPerLane] {};
    uint32_t mods[numLanes][wordsPerLane] {};

    SlotData()
    {
        for (int i = 0; i < numLanes; ++i)
            loop[i] = 16;
    }
};

inline int packed2 (const uint32_t words[SlotData::wordsPerLane], int i)
{
    if (i < 0 || i >= SlotData::maxSteps)
        return 0;
    const int sh = (i % 16) * 2;
    return (int) ((words[i / 16] >> sh) & 3u);
}

inline void pack2 (uint32_t words[SlotData::wordsPerLane], int i, int v)
{
    if (i < 0 || i >= SlotData::maxSteps)
        return;
    const int sh = (i % 16) * 2;
    const uint32_t mask = 3u << sh;
    words[i / 16] = (words[i / 16] & ~mask) | (((uint32_t) (v & 3)) << sh);
}

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
    if (lane < 0 || lane >= numLanes)
        return 0;
    return packed2 (s.words[lane], i);
}

inline void setStep (SlotData& s, int lane, int i, int v)
{
    if (lane < 0 || lane >= numLanes)
        return;
    pack2 (s.words[lane], i, v);
}

inline int getMod (const SlotData& s, int lane, int i)
{
    if (lane < 0 || lane >= numLanes)
        return 0;
    return packed2 (s.mods[lane], i);
}

inline void setMod (SlotData& s, int lane, int i, int v)
{
    if (lane < 0 || lane >= numLanes)
        return;
    pack2 (s.mods[lane], i, v);
}

// off → ghost → on → accent → off. Keeps 1=on and 2=accent for old banks.
inline int nextVel (int v)
{
    if (v == 0)
        return 3;
    if (v == 3)
        return 1;
    if (v == 1)
        return 2;
    return 0;
}

inline int quieterVel (int v) { return v >= 3 ? 2 : 1; }

// Grow 16→32/64 by tiling the current length so extra bars are not empty.
inline void setPatternLen (SlotData& s, int newLen)
{
    const int old = clampLen ((int) s.len);
    newLen = clampLen (newLen);
    if (newLen > old)
        for (int lane = 0; lane < numLanes; ++lane)
            for (int i = old; i < newLen; ++i)
            {
                setStep (s, lane, i, getStep (s, lane, i % old));
                setMod (s, lane, i, getMod (s, lane, i % old));
            }
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
        {
            setStep (s, lane, i, 0);
            setMod (s, lane, i, 0);
        }
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
            setMod (s, lane, i, 0);
        }
}

inline void randomizeLaneSteps (SlotData& s, int lane, uint32_t seed)
{
    if (lane < 0 || lane >= numLanes)
        return;
    const int n = clampLen ((int) s.len);
    uint32_t x = seed ? seed : 1u;
    for (int i = 0; i < n; ++i)
    {
        x = x * 1664525u + 1013904223u;
        const int r = (int) (x % 100u);
        setStep (s, lane, i, r < 10 ? 2 : (r < 45 ? 1 : 0));
        setMod (s, lane, i, 0);
    }
}

// n = lane loop. k = on-count in that loop, or n/2 if empty.
inline void euclideanLaneSteps (SlotData& s, int lane)
{
    if (lane < 0 || lane >= numLanes)
        return;
    const int nPat = clampLen ((int) s.len);
    const int n = std::min (nPat, std::max (1, (int) s.loop[lane]));
    int k = 0;
    for (int i = 0; i < n; ++i)
        if (getStep (s, lane, i) > 0)
            ++k;
    if (k == 0)
        k = std::max (1, n / 2);
    k = std::min (k, n);
    int acc = 0;
    bool first = true;
    for (int i = 0; i < nPat; ++i)
    {
        int v = 0;
        if (i < n)
        {
            acc += k;
            if (acc >= n)
            {
                acc -= n;
                v = first ? 2 : 1;
                first = false;
            }
        }
        setStep (s, lane, i, v);
        setMod (s, lane, i, 0);
    }
}

inline void copyLaneGrid (SlotData& dest, int destLane, const SlotData& src, int srcLane)
{
    if (destLane < 0 || destLane >= numLanes || srcLane < 0 || srcLane >= numLanes)
        return;
    dest.loop[destLane] = src.loop[srcLane];
    std::memcpy (dest.words[destLane], src.words[srcLane], sizeof (src.words[srcLane]));
    std::memcpy (dest.mods[destLane], src.mods[srcLane], sizeof (src.mods[srcLane]));
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
    int vel = 2; // 1 ghost, 2 on, 3 accent
};

struct Sequencer
{
    static constexpr int numBanks = 3;
    static constexpr int patternsPerBank = 12;
    static constexpr int fillPattern = 11; // pattern 12, 0-based
    static constexpr int fillNote = 60;    // C3, first key above the 36 slot map
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
                 const float swing[numLanes], const float probPct[numLanes],
                 uint8_t enabled, Hit* hits, int maxHits)
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

        auto push = [&] (double t, int lane, int vel)
        {
            for (int k = -1; k <= 1; ++k)
            {
                const double tt = t + (double) k * loopSamps;
                if (tt < start || tt >= end)
                    continue;
                if (nHits >= maxHits)
                    return false;
                hits[nHits++] = { (int) (tt - start), lane, vel };
            }
            return true;
        };

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
                const float pct = probPct != nullptr ? probPct[lane] : 100.f;
                if (pct < 100.f)
                {
                    if (pct <= 0.f)
                        continue;
                    rng = rng * 1664525u + 1013904223u;
                    if ((int) (rng % 100u) >= (int) pct)
                        continue;
                }

                double t = (double) st * sps;
                if ((ls & 1) != 0)
                    t += swingOffset (swing[lane], sps);

                const int mod = getMod (slot, lane, ls);
                int nRep = 1;
                double frac[4] { 0.0, 0.0, 0.0, 0.0 };
                int vel[4] { v, v, v, v };
                if (mod == 1)
                {
                    nRep = 2;
                    frac[1] = 1.0 / 6.0;
                    vel[1] = quieterVel (v);
                }
                else if (mod == 2)
                {
                    nRep = 2;
                    frac[1] = 0.5;
                }
                else if (mod == 3)
                {
                    nRep = 4;
                    frac[1] = 0.25;
                    frac[2] = 0.5;
                    frac[3] = 0.75;
                }

                for (int r = 0; r < nRep; ++r)
                    if (! push (t + frac[r] * sps, lane, vel[r]))
                        return nHits;
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
    uint32_t rng = 1u;
};

static_assert (sizeof (factoryPats) / sizeof (factoryPats[0]) == Sequencer::numBanks
                   && sizeof (factoryPats[0]) / sizeof (factoryPats[0][0]) == Sequencer::patternsPerBank,
               "factoryPats needs one groove per bank/pattern slot");

} // namespace hkr
