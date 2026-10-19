#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>

namespace hkr
{

struct OutputMeter
{
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
            const float a = std::abs (0.5f * (l[i] + r[i]));
            if (a > p)
                p = a;
            if (a >= 1.f)
                clip = true;
        }
        peak.store (p, std::memory_order_relaxed);
        if (clip)
            clipArmed.store (true, std::memory_order_relaxed);
    }

    float lastPeak() const { return peak.load (std::memory_order_relaxed); }

    bool takeClip()
    {
        return clipArmed.exchange (false, std::memory_order_relaxed);
    }

private:
    std::atomic<float> peak { 0.f };
    std::atomic<bool> clipArmed { false };
};

} // namespace hkr
