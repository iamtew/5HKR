#pragma once

#include "EqBiquad.h"
#include "Sequencer.h"

#include <cmath>
#include <cstdint>

namespace hkr
{

inline constexpr float mixerFilterDead = 0.02f;
inline constexpr float mixerLoHz = 250.f;
inline constexpr float mixerMidHz = 1000.f;
inline constexpr float mixerHiHz = 4000.f;

inline float mixerDbToLin (float db)
{
    return std::pow (10.f, db * 0.05f);
}

// Left = HP (cutoff climbs), right = LP (cutoff falls). |amount| < dead = bypass.
inline bool mixerFilterBypass (float amount)
{
    return std::fabs (amount) < mixerFilterDead;
}

inline float mixerFilterHz (float amount, float sr)
{
    const float ny = (sr > 1.f ? sr : 44100.f) * 0.45f;
    const float lo = 20.f;
    float t = std::fabs (amount);
    if (t > 1.f)
        t = 1.f;
    const float ratio = ny / lo;
    if (amount < 0.f)
        return lo * std::pow (ratio, t);
    return lo * std::pow (ratio, 1.f - t);
}

inline void mixerPanGains (float pan, float& gL, float& gR)
{
    if (pan < -1.f)
        pan = -1.f;
    if (pan > 1.f)
        pan = 1.f;
    const float theta = (pan + 1.f) * 0.7853981633974483f; // (pan+1) * pi/4
    gL = std::cos (theta);
    gR = std::sin (theta);
}

// 0xff = all lanes (no solo). Else only bits that are soloed.
inline std::uint8_t mixerSoloMask (const bool* solo)
{
    std::uint8_t any = 0;
    if (solo == nullptr)
        return 0xff;
    for (int i = 0; i < numLanes; ++i)
        if (solo[i])
            any = (std::uint8_t) (any | (1u << i));
    return any == 0 ? (std::uint8_t) 0xff : any;
}

struct Df1
{
    float z1 = 0.f, z2 = 0.f;

    float process (float x, const Biquad& c)
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset()
    {
        z1 = 0.f;
        z2 = 0.f;
    }
};

struct ChannelStrip
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? (float) sampleRate : 44100.f;
        lo.reset();
        mid.reset();
        hi.reset();
        filt.reset();
        loC = {};
        midC = {};
        hiC = {};
        filtC = {};
        trim = 1.f;
        fader = 1.f;
        panL = 0.70710678f;
        panR = 0.70710678f;
    }

    void set (float trimDb, float loDb, float midDb, float hiDb,
              float filterAmt, float faderAmt, float pan)
    {
        trim = mixerDbToLin (trimDb);
        fader = faderAmt < 0.f ? 0.f : (faderAmt > 1.f ? 1.f : faderAmt);
        mixerPanGains (pan, panL, panR);
        loC = eqBiquad (eqTypeLowShelf, sr, mixerLoHz, 0.7f, loDb);
        midC = eqBiquad (eqTypePeak, sr, mixerMidHz, 0.8f, midDb);
        hiC = eqBiquad (eqTypeHighShelf, sr, mixerHiHz, 0.7f, hiDb);
        if (mixerFilterBypass (filterAmt))
            filtC = {};
        else if (filterAmt < 0.f)
            filtC = eqBiquad (eqTypeHP, sr, mixerFilterHz (filterAmt, sr), 0.707f, 0.f);
        else
            filtC = eqBiquad (eqTypeLP, sr, mixerFilterHz (filterAmt, sr), 0.707f, 0.f);
    }

    float process (float x)
    {
        x *= trim;
        x = lo.process (x, loC);
        x = mid.process (x, midC);
        x = hi.process (x, hiC);
        x = filt.process (x, filtC);
        return x * fader;
    }

    float panL = 0.70710678f;
    float panR = 0.70710678f;

private:
    float sr = 44100.f;
    float trim = 1.f;
    float fader = 1.f;
    Df1 lo, mid, hi, filt;
    Biquad loC, midC, hiC, filtC;
};

struct LaneMixer
{
    void prepare (double sampleRate)
    {
        for (int i = 0; i < numLanes; ++i)
            strip[i].prepare (sampleRate);
    }

    ChannelStrip strip[numLanes];
};

} // namespace hkr
