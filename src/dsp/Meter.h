#pragma once

#include "Sequencer.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace hkr
{

inline constexpr float meterFloorDb = -48.f;
inline constexpr float meterReleaseSec = 0.4f;

// -48 dBFS = empty, 0 dBFS = full. Linear 0..1 hides everything under about -12 dB.
inline float meterToPos (float lin)
{
    if (lin < 1.0e-5f)
        return 0.f;
    const float db = 20.f * std::log10 (lin);
    const float pos = (db - meterFloorDb) / -meterFloorDb;
    return pos < 0.f ? 0.f : (pos > 1.f ? 1.f : pos);
}

inline float meterRelease (float hold, int numSamples, double sr)
{
    if (hold <= 1.0e-6f || sr < 1.0 || numSamples <= 0)
        return 0.f;
    const float k = std::exp (-(float) numSamples / ((float) sr * meterReleaseSec));
    return hold * k;
}

} // namespace hkr

#if __has_include(<JuceHeader.h>)
#include <JuceHeader.h>

namespace hkr
{
struct OutputMeter
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        hold = 0.f;
        peak.store (0.f, std::memory_order_relaxed);
    }

    void push (const juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        if (n <= 0 || buffer.getNumChannels() <= 0)
            return;

        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
        float p = 0.f;
        bool clip = false;
        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (l[i]), std::abs (r[i]));
            if (a > p)
                p = a;
            if (a >= 1.f)
                clip = true;
        }
        if (p > hold)
            hold = p;
        peak.store (hold, std::memory_order_relaxed);
        hold = meterRelease (hold, n, sr);
        if (clip)
            clipArmed.store (true, std::memory_order_relaxed);
    }

    float lastPeak() const { return peak.load (std::memory_order_relaxed); }

    bool takeClip()
    {
        return clipArmed.exchange (false, std::memory_order_relaxed);
    }

private:
    double sr = 44100.0;
    float hold = 0.f;
    std::atomic<float> peak { 0.f };
    std::atomic<bool> clipArmed { false };
};

} // namespace hkr
#endif

namespace hkr
{

// Per-lane post-fader peak. Same hold as the master so a 5 ms hat is still on screen.
struct LaneMeters
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        for (int i = 0; i < numLanes; ++i)
        {
            hold[i] = 0.f;
            peak[i].store (0.f, std::memory_order_relaxed);
        }
    }

    void pushBlock (const float* blockPeak, int numSamples)
    {
        if (blockPeak == nullptr)
            return;
        for (int i = 0; i < numLanes; ++i)
        {
            if (blockPeak[i] > hold[i])
                hold[i] = blockPeak[i];
            peak[i].store (hold[i], std::memory_order_relaxed);
            hold[i] = meterRelease (hold[i], numSamples, sr);
        }
    }

    float last (int lane) const
    {
        if (lane < 0 || lane >= numLanes)
            return 0.f;
        return peak[lane].load (std::memory_order_relaxed);
    }

private:
    double sr = 44100.0;
    float hold[numLanes] {};
    std::atomic<float> peak[numLanes] {};
};

} // namespace hkr
