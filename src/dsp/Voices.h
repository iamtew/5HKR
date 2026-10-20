#pragma once

#include "Sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hkr
{

// Analog-ish hits. Accent = louder and a bit brighter. Tune is semitones.
// Kits are coefficient tables, not four engines. No sample ROM.
struct Voices
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        rng = 0xA5A5u;
        for (int i = 0; i < numLanes; ++i)
        {
            env[i] = 0.f;
            bright[i] = 0.f;
            age[i] = 0;
            lp[i] = 0.f;
            bp[i] = 0.f;
            for (int o = 0; o < 6; ++o)
                osc[i][o] = 0.f;
        }
    }

    void trigger (int lane, bool accent)
    {
        if (lane < 0 || lane >= numLanes)
            return;
        env[lane] = accent ? 1.f : 0.65f;
        bright[lane] = accent ? 1.f : 0.f;
        age[lane] = 0;
        for (int o = 0; o < 6; ++o)
            osc[lane][o] = 0.f;
    }

    void setTune (const float semi[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            tune[i] = semi[i];
    }

    void setKit (const int k[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            kit[i] = std::clamp (k[i], 0, numKits - 1);
    }

    void setEdit (const float decay[numLanes], const float click[numLanes],
                  const float hpAmt[numLanes], const float lpAmt[numLanes],
                  const float gain[numLanes], const float drive[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
        {
            editDecay[i] = std::clamp (decay[i], 0.f, 1.f);
            editClick[i] = std::clamp (click[i], 0.f, 1.f);
            editHp[i] = std::clamp (hpAmt[i], 0.f, 1.f);
            editLp[i] = std::clamp (lpAmt[i], 0.f, 1.f);
            editGain[i] = std::clamp (gain[i], 0.f, 1.f);
            editDrive[i] = std::clamp (drive[i], 0.f, 1.f);
        }
    }

    void process (float laneOut[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            laneOut[i] = render (i);
    }

    float envelope (int lane) const { return env[lane]; }
    int samplesSinceTrigger (int lane) const { return age[lane]; }

private:
    enum Kind : uint8_t { Noise, Metal, Tone };

    struct Spec
    {
        Kind kind;
        float decay;
        float hp;    // one-pole mix 0..1
        float lp;    // bandpass lowpass after HP; 1 = skip
        float gain;
        float click;
        float bits;  // 0 = full; else round(n * bits) / bits
        uint8_t nOsc;
        bool square;
        float hz[6];
    };

    float noise()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) ((int) (rng >> 16) - 32768) * (1.f / 32768.f);
    }

    float quant (float x, float bits) const
    {
        if (bits <= 0.f)
            return x;
        return std::round (x * bits) / bits;
    }

    float ratio (int lane) const
    {
        return std::pow (2.f, tune[lane] / 12.f);
    }

    static float around (float spec, float amt)
    {
        return spec * (0.4f + 1.2f * amt);
    }

    static constexpr float kTwoPi = 6.2831853f;

    float tickOsc (int lane, int o, float hz)
    {
        osc[lane][o] += (float) (2.0 * 3.141592653589793 * (double) hz / sr);
        if (osc[lane][o] > kTwoPi)
            osc[lane][o] -= kTwoPi;
        return osc[lane][o];
    }

    float mixOsc (int lane, const Spec& s, float r)
    {
        const int n = std::max (1, (int) s.nOsc);
        float sum = 0.f;
        for (int o = 0; o < n; ++o)
        {
            const float ph = tickOsc (lane, o, s.hz[o] * r);
            sum += s.square ? (ph < 3.14159265f ? 1.f : -1.f) : std::sin (ph);
        }
        return sum / (float) n;
    }

    float filter (int lane, float x, float hpAmt, float lpAmt)
    {
        hpAmt = std::clamp (hpAmt, 0.02f, 0.9f);
        lp[lane] += hpAmt * (x - lp[lane]);
        float y = x - lp[lane];
        if (lpAmt < 0.99f)
        {
            lpAmt = std::clamp (lpAmt, 0.02f, 0.98f);
            bp[lane] += lpAmt * (y - bp[lane]);
            y = bp[lane];
        }
        return y;
    }

    float render (int lane)
    {
        const Spec& s = kSpec[kit[lane]][lane];
        // 0.5 = kit spec. Decay exponent floor 0.5 so tails cannot hang forever.
        env[lane] *= std::pow (s.decay, 2.f - 1.5f * editDecay[lane]);
        ++age[lane];
        const float e = env[lane];
        const float r = ratio (lane);
        const float hpAmt = around (s.hp, editHp[lane]) * r * (1.f + 0.2f * bright[lane]);
        float x = 0.f;
        if (s.kind == Noise)
            x = quant (noise(), s.bits);
        else
            x = mixOsc (lane, s, r);
        x = filter (lane, x, hpAmt, around (s.lp, editLp[lane]));
        x += around (s.click, editClick[lane]) * e * e * noise();
        x *= e * around (s.gain, editGain[lane]);
        if (editDrive[lane] > 0.f)
            x = std::tanh (x * (1.f + 4.f * editDrive[lane]));
        return x;
    }

    // Inspired-by analog models. Index = Kit enum.
    static constexpr Spec kSpec[numKits][numLanes] = {
        // 707 — short, thin, PCM-ish bandpass
        {
            { Noise, 0.978f, 0.42f, 0.55f, 0.72f, 0.35f, 0.f, 0, false, {} },           // CHH
            { Noise, 0.990f, 0.32f, 0.50f, 0.62f, 0.22f, 0.f, 0, false, {} },           // OHH
            { Noise, 0.9978f, 0.18f, 0.40f, 0.55f, 0.18f, 0.f, 0, false, {} },          // Crash
            { Noise, 0.9965f, 0.24f, 0.45f, 0.55f, 0.15f, 0.f, 0, false, {} },          // Cym
            { Noise, 0.930f, 0.50f, 0.62f, 0.70f, 0.40f, 0.f, 0, false, {} },           // Shaker
            { Tone,  0.975f, 0.08f, 1.00f, 0.78f, 0.20f, 0.f, 1, true, { 2400.f } },    // Clave
            { Tone,  0.984f, 0.06f, 1.00f, 0.62f, 0.08f, 0.f, 2, true, { 587.f, 845.f } }, // Cow
            { Tone,  0.970f, 0.12f, 1.00f, 0.70f, 0.45f, 0.f, 1, false, { 420.f } },    // Rim
        },
        // 808 — square stacks, analog tails
        {
            { Metal, 0.992f, 0.38f, 1.00f, 0.58f, 0.12f, 0.f, 6, true,
              { 205.f, 304.f, 370.f, 523.f, 540.f, 800.f } },
            { Metal, 0.9978f, 0.28f, 1.00f, 0.50f, 0.08f, 0.f, 6, true,
              { 205.f, 304.f, 370.f, 523.f, 540.f, 800.f } },
            { Metal, 0.9995f, 0.16f, 1.00f, 0.42f, 0.06f, 0.f, 6, true,
              { 205.f, 304.f, 370.f, 523.f, 540.f, 800.f } },
            { Metal, 0.9992f, 0.22f, 1.00f, 0.45f, 0.06f, 0.f, 6, true,
              { 205.f, 304.f, 370.f, 523.f, 540.f, 800.f } },
            { Noise, 0.940f, 0.48f, 1.00f, 0.68f, 0.30f, 0.f, 0, false, {} },
            { Tone,  0.988f, 0.05f, 1.00f, 0.72f, 0.18f, 0.f, 1, false, { 2500.f } },
            { Metal, 0.993f, 0.07f, 1.00f, 0.58f, 0.05f, 0.f, 2, true, { 540.f, 800.f } },
            { Tone,  0.982f, 0.10f, 1.00f, 0.62f, 0.40f, 0.f, 1, false, { 380.f } },
        },
        // 909 — gated noise hats, sizzle crash
        {
            { Noise, 0.972f, 0.40f, 0.58f, 0.75f, 0.42f, 0.f, 0, false, {} },
            { Noise, 0.9960f, 0.26f, 0.48f, 0.58f, 0.22f, 0.f, 0, false, {} },
            { Noise, 0.9993f, 0.14f, 0.38f, 0.50f, 0.16f, 0.f, 0, false, {} },
            { Tone,  0.9988f, 0.20f, 0.42f, 0.48f, 0.20f, 0.f, 3, false, { 320.f, 540.f, 890.f } },
            { Noise, 0.948f, 0.46f, 0.60f, 0.66f, 0.28f, 0.f, 0, false, {} },
            { Tone,  0.980f, 0.10f, 1.00f, 0.74f, 0.35f, 0.f, 1, true, { 2700.f } },
            { Tone,  0.990f, 0.08f, 1.00f, 0.55f, 0.10f, 0.f, 2, false, { 555.f, 835.f } },
            { Tone,  0.978f, 0.14f, 1.00f, 0.68f, 0.50f, 0.f, 1, false, { 450.f } },
        },
        // Linn — darker, grainy, woody
        {
            { Noise, 0.989f, 0.22f, 0.40f, 0.70f, 0.28f, 48.f, 0, false, {} },
            { Noise, 0.9962f, 0.16f, 0.35f, 0.58f, 0.16f, 48.f, 0, false, {} },
            { Noise, 0.9991f, 0.10f, 0.28f, 0.48f, 0.12f, 32.f, 0, false, {} },
            { Tone,  0.9985f, 0.12f, 0.32f, 0.50f, 0.10f, 0.f, 2, false, { 280.f, 420.f } },
            { Noise, 0.910f, 0.38f, 0.55f, 0.72f, 0.50f, 24.f, 0, false, {} },
            { Tone,  0.982f, 0.06f, 1.00f, 0.70f, 0.22f, 0.f, 1, false, { 2100.f } },
            { Tone,  0.991f, 0.05f, 1.00f, 0.60f, 0.08f, 0.f, 2, false, { 480.f, 720.f } },
            { Tone,  0.968f, 0.18f, 1.00f, 0.72f, 0.55f, 0.f, 1, false, { 340.f } },
        },
    };

    double sr = 44100.0;
    uint32_t rng = 1;
    int kit[numLanes] { kit808, kit808, kit808, kit808, kit808, kit808, kit808, kit808 };
    float env[numLanes] {};
    int age[numLanes] {};
    float bright[numLanes] {};
    float osc[numLanes][6] {};
    float lp[numLanes] {};
    float bp[numLanes] {};
    float tune[numLanes] {};
    float editDecay[numLanes] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float editClick[numLanes] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float editHp[numLanes] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float editLp[numLanes] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float editGain[numLanes] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    float editDrive[numLanes] {};
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
