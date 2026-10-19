#pragma once

#include "Sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hkr
{

// Tiny analog-ish hits. Accent = louder and a bit brighter.
struct Voices
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        rng = 0xA5A5u;
        for (int i = 0; i < numLanes; ++i)
        {
            env[i] = 0.f;
            ph[i] = 0.f;
            ph2[i] = 0.f;
            lp[i] = 0.f;
        }
    }

    void trigger (int lane, bool accent)
    {
        if (lane < 0 || lane >= numLanes)
            return;
        env[lane] = accent ? 1.f : 0.65f;
        bright[lane] = accent ? 1.f : 0.f;
        ph[lane] = 0.f;
        ph2[lane] = 0.f;
    }

    // Semitones. 0 = 12 o'clock.
    void setTune (const float semi[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            tune[i] = semi[i];
    }

    void process (float laneOut[numLanes])
    {
        laneOut[chh] = noiseHit (chh, 0.992f, 0.35f + 0.15f * bright[chh]);
        laneOut[ohh] = noiseHit (ohh, 0.9975f, 0.22f + 0.1f * bright[ohh]);
        laneOut[crash] = noiseHit (crash, 0.9994f, 0.12f);
        laneOut[cymbal] = noiseHit (cymbal, 0.9991f, 0.18f);
        laneOut[shaker] = noiseHit (shaker, 0.96f, 0.45f);
        laneOut[clave] = sineHit (clave, 2450.f, 0.991f, 0.35f);
        laneOut[cowbell] = cow();
        laneOut[rim] = rimHit();
    }

private:
    float noise()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) ((int) (rng >> 16) - 32768) * (1.f / 32768.f);
    }

    float ratio (int lane) const
    {
        return std::pow (2.f, tune[lane] / 12.f);
    }

    float noiseHit (int lane, float decay, float hp)
    {
        env[lane] *= decay;
        hp = std::clamp (hp * ratio (lane), 0.02f, 0.9f);
        const float n = noise();
        lp[lane] += hp * (n - lp[lane]);
        return (n - lp[lane]) * env[lane] * 0.35f;
    }

    float sineHit (int lane, float hz, float decay, float gain)
    {
        env[lane] *= decay;
        hz *= ratio (lane);
        ph[lane] += (float) (2.0 * 3.141592653589793 * (double) hz / sr);
        if (ph[lane] > 6.2831853f)
            ph[lane] -= 6.2831853f;
        return std::sin (ph[lane]) * env[lane] * gain;
    }

    float cow()
    {
        env[cowbell] *= 0.993f;
        const float e = env[cowbell];
        const float r = ratio (cowbell);
        ph[cowbell] += (float) (2.0 * 3.141592653589793 * 540.0 * (double) r / sr);
        ph2[cowbell] += (float) (2.0 * 3.141592653589793 * 800.0 * (double) r / sr);
        return (std::sin (ph[cowbell]) + 0.7f * std::sin (ph2[cowbell])) * e * 0.22f;
    }

    float rimHit()
    {
        env[rim] *= 0.985f;
        const float r = ratio (rim);
        const float click = env[rim] * env[rim] * noise() * 0.25f;
        ph[rim] += (float) (2.0 * 3.141592653589793 * 380.0 * (double) r / sr);
        return std::sin (ph[rim]) * env[rim] * 0.28f + click;
    }

    double sr = 44100.0;
    uint32_t rng = 1;
    float env[numLanes] {};
    float bright[numLanes] {};
    float ph[numLanes] {};
    float ph2[numLanes] {};
    float lp[numLanes] {};
    float tune[numLanes] {};
};

struct Ducker
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        env = 0.f;
    }

    // atk/rel in milliseconds. amount 0..1.
    float gain (float scAbs, float thresh, float amount, float atkMs, float relMs)
    {
        const float atk = 1.f - std::exp (-1.f / (float) (sr * std::max (0.001f, atkMs) * 0.001f));
        const float rel = 1.f - std::exp (-1.f / (float) (sr * std::max (1.f, relMs) * 0.001f));
        env += (scAbs - env) * (scAbs > env ? atk : rel);
        if (env <= thresh)
            return 1.f;
        const float over = env - thresh;
        return 1.f / (1.f + amount * over * 12.f);
    }

private:
    double sr = 44100.0;
    float env = 0.f;
};

// One-pole HP then LP on the sidechain key. ponytail: 6 dB/oct; SVF if a kick still leaks hats.
struct ScFilter
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        lp = 0.f;
        hpLp = 0.f;
    }

    float process (float x, float hpHz, float lpHz)
    {
        const float hpC = coeff (hpHz);
        hpLp += hpC * (x - hpLp);
        const float hp = x - hpLp;
        const float lpC = coeff (lpHz);
        lp += lpC * (hp - lp);
        return lp;
    }

private:
    float coeff (float hz) const
    {
        const float f = std::clamp (hz, 1.f, (float) (sr * 0.45));
        return 1.f - std::exp (-6.2831853f * f / (float) sr);
    }

    double sr = 44100.0;
    float lp = 0.f;
    float hpLp = 0.f;
};

} // namespace hkr
