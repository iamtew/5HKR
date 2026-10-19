#pragma once

#include "../dsp/Sequencer.h"

#include <cstdint>
#include <cstring>

namespace ParamID
{
constexpr const char* volume     = "volume";
constexpr const char* seqTempo   = "seqTempo";
constexpr const char* seqBank    = "seqBank";
constexpr const char* seqPattern = "seqPattern";

constexpr const char* scOn     = "scOn";
constexpr const char* scListen = "scListen";
constexpr const char* scHp     = "scHp";
constexpr const char* scLp     = "scLp";
constexpr const char* scThresh = "scThresh";
constexpr const char* scAmount = "scAmount";
constexpr const char* scAttack = "scAttack";
constexpr const char* scRelease = "scRelease";
constexpr const char* scMix    = "scMix";

inline constexpr const char* laneOn[hkr::numLanes] = {
    "laneOn0", "laneOn1", "laneOn2", "laneOn3",
    "laneOn4", "laneOn5", "laneOn6", "laneOn7"
};
inline constexpr const char* laneSwing[hkr::numLanes] = {
    "laneSwing0", "laneSwing1", "laneSwing2", "laneSwing3",
    "laneSwing4", "laneSwing5", "laneSwing6", "laneSwing7"
};
inline constexpr const char* laneTune[hkr::numLanes] = {
    "laneTune0", "laneTune1", "laneTune2", "laneTune3",
    "laneTune4", "laneTune5", "laneTune6", "laneTune7"
};
inline constexpr const char* laneSend[hkr::numLanes] = {
    "laneSend0", "laneSend1", "laneSend2", "laneSend3",
    "laneSend4", "laneSend5", "laneSend6", "laneSend7"
};

inline constexpr int fxCount = 5;
inline constexpr const char* fxNames[fxCount] = {
    "Chorus", "Delay", "Flanger", "Phaser", "Reverb"
};
inline constexpr const char* fxOnIds[fxCount] = {
    "fxChoOn", "fxDlyOn", "fxFlaOn", "fxPhaOn", "fxRevOn"
};

inline constexpr const char* fxChoRate = "fxChoRate";
inline constexpr const char* fxChoDepth = "fxChoDepth";
inline constexpr const char* fxChoMix = "fxChoMix";
inline constexpr const char* fxChoFb = "fxChoFb";

inline constexpr const char* fxDlyTime = "fxDlyTime";
inline constexpr const char* fxDlyFb = "fxDlyFb";
inline constexpr const char* fxDlyMix = "fxDlyMix";
inline constexpr const char* fxDlyCut = "fxDlyCut";
inline constexpr const char* fxDlySync = "fxDlySync";
inline constexpr const char* fxDlyDiv = "fxDlyDiv";

inline constexpr const char* fxFlaRate = "fxFlaRate";
inline constexpr const char* fxFlaDepth = "fxFlaDepth";
inline constexpr const char* fxFlaFb = "fxFlaFb";
inline constexpr const char* fxFlaMix = "fxFlaMix";

inline constexpr const char* fxPhaRate = "fxPhaRate";
inline constexpr const char* fxPhaDepth = "fxPhaDepth";
inline constexpr const char* fxPhaFb = "fxPhaFb";
inline constexpr const char* fxPhaMix = "fxPhaMix";
inline constexpr const char* fxPhaCentre = "fxPhaCentre";

inline constexpr const char* fxRevSize = "fxRevSize";
inline constexpr const char* fxRevDamp = "fxRevDamp";
inline constexpr const char* fxRevMix = "fxRevMix";
inline constexpr const char* fxRevWidth = "fxRevWidth";

inline constexpr int eqBandCount = 5;
inline constexpr const char* postEqOn = "postEqOn";
inline constexpr const char* postEqType[eqBandCount] = {
    "postEq1Type", "postEq2Type", "postEq3Type", "postEq4Type", "postEq5Type"
};
inline constexpr const char* postEqFreq[eqBandCount] = {
    "postEq1Freq", "postEq2Freq", "postEq3Freq", "postEq4Freq", "postEq5Freq"
};
inline constexpr const char* postEqGain[eqBandCount] = {
    "postEq1Gain", "postEq2Gain", "postEq3Gain", "postEq4Gain", "postEq5Gain"
};
inline constexpr const char* postEqQ[eqBandCount] = {
    "postEq1Q", "postEq2Q", "postEq3Q", "postEq4Q", "postEq5Q"
};
}

namespace hkr
{

inline constexpr int fxCount = ParamID::fxCount;

inline int fillDefaultFxOrder (int* types)
{
    if (types == nullptr)
        return 0;
    for (int i = 0; i < fxCount; ++i)
        types[i] = i;
    return fxCount;
}

inline int completeFxOrder (int* types, int n)
{
    if (types == nullptr)
        return 0;
    bool seen[fxCount] {};
    int tmp[fxCount];
    int m = 0;
    n = n < 0 ? 0 : (n > fxCount ? fxCount : n);
    for (int i = 0; i < n; ++i)
    {
        const int t = types[i];
        if (t < 0 || t >= fxCount || seen[t])
            continue;
        seen[t] = true;
        tmp[m++] = t;
    }
    for (int t = 0; t < fxCount; ++t)
        if (! seen[t])
            tmp[m++] = t;
    for (int i = 0; i < m; ++i)
        types[i] = tmp[i];
    return m;
}

inline std::uint64_t packFxOrder (const int* types, int n)
{
    std::uint64_t packed = 0;
    if (types == nullptr)
        return 0;
    n = n < 0 ? 0 : (n > fxCount ? fxCount : n);
    for (int i = 0; i < n; ++i)
    {
        const int t = types[i];
        if (t < 0 || t >= fxCount)
            continue;
        packed |= (std::uint64_t) ((t & 15) + 1) << (i * 4);
    }
    return packed;
}

inline int unpackFxOrder (std::uint64_t packed, int* types)
{
    if (types == nullptr)
        return 0;
    int n = 0;
    bool seen[fxCount] {};
    for (int i = 0; i < fxCount; ++i)
    {
        const int v = (int) ((packed >> (i * 4)) & 15);
        if (v == 0)
            break;
        const int t = v - 1;
        if (t < 0 || t >= fxCount || seen[t])
            continue;
        seen[t] = true;
        types[n++] = t;
    }
    return n;
}

inline int parseFxOrder (const char* s, int* types)
{
    int n = 0;
    if (s == nullptr || types == nullptr)
        return 0;
    bool seen[fxCount] {};
    while (*s != 0 && n < fxCount)
    {
        const int t = *s - '0';
        ++s;
        if (t < 0 || t >= fxCount || seen[t])
            continue;
        seen[t] = true;
        types[n++] = t;
    }
    return n;
}

inline void formatFxOrder (std::uint64_t packed, char* buf, int bufSize)
{
    if (buf == nullptr || bufSize <= 0)
        return;
    int types[fxCount];
    const int n = unpackFxOrder (packed, types);
    int w = 0;
    for (int i = 0; i < n && w + 2 < bufSize; ++i)
    {
        if (i > 0)
            buf[w++] = ',';
        buf[w++] = (char) ('0' + types[i]);
    }
    buf[w] = 0;
}

} // namespace hkr
