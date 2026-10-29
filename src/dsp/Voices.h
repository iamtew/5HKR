#pragma once

#include "Sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hkr
{

// Analog-ish hits. Kits 707/808/909/Linn are coefficient tables, not four engines.
// kitSample plays a caller-owned mono buffer (one-shot). That pointer is only read.
//
// Per lane, per sample:
//   osc stack (+ pitch drop) and/or noise  ->  resonant SVF (BP or HP)
//   -> one-pole LP (tone)  -> + click burst  -> snap + tail envelope
//   -> gain  -> tanh saturation (kit sat + Drive)  -> bit-round
//
// Every Spec time is ms to -60 dB and every frequency is Hz, so a hit sounds
// the same at 44.1k and 96k. Coefficients are rebuilt once per block (setters
// mark dirty, process() refreshes), so render() has no pow / exp / tan.
// Accent = louder, more snap, filter ~15% brighter. Tune is semitones and moves
// oscillators and the SVF together.
struct Voices
{
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        rng = 0xA5A5u;
        for (auto& s : st)
            s = State {};
        for (int i = 0; i < numLanes; ++i)
        {
            sampStart[i] = 0.f;
            sampEnd[i] = 1.f;
            sampAtk[i] = 0.f;
            sampRel[i] = 0.f;
            sampRev[i] = false;
            sp[i] = SampPlay {};
            view[i] = SampleView {};
        }
        clickMul = decayMul (4.f);
        dirty = true;
    }

    // Pointers stay valid until the owner publishes a different buffer.
    struct SampleView
    {
        const float* data = nullptr;
        int frames = 0;
        double fileRate = 44100.0;
    };

    void setSamples (const SampleView v[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            view[i] = v[i];
    }

    // Start/end/atk/rel are fractions of the file / played region. Latched on the next trigger.
    void setSampleRegion (const float start[numLanes], const float end[numLanes],
                          const bool rev[numLanes],
                          const float atk[numLanes], const float rel[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
        {
            sampStart[i] = std::clamp (start[i], 0.f, 1.f);
            sampEnd[i] = std::clamp (end[i], 0.f, 1.f);
            sampRev[i] = rev[i];
            sampAtk[i] = std::clamp (atk[i], 0.f, 1.f);
            sampRel[i] = std::clamp (rel[i], 0.f, 1.f);
        }
    }

    void trigger (int lane, bool accent)
    {
        if (lane < 0 || lane >= numLanes)
            return;
        auto& s = st[lane];
        s.age = 0;
        if (kit[lane] == kitSample)
        {
            triggerSample (lane, accent);
            return;
        }
        sp[lane].on = false;
        s.level = accent ? 1.f : 0.65f;
        s.accent = accent;
        s.tail = 1.f;
        s.snap = accent ? 1.3f : 1.f;
        s.click = 1.f;
        s.pitch = 1.f;
        // Squares are free-running like the 808 metal oscillators (each hit
        // lands on a different phase). Sines are struck resonators: reset.
        if (kit[lane] >= 0 && kit[lane] < numSynthKits && ! kSpec[kit[lane]][lane].square)
            for (auto& p : s.ph)
                p = 0.f;
    }

    void setTune (const float semi[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            tune[i] = semi[i];
        dirty = true;
    }

    void setKit (const int k[numLanes])
    {
        for (int i = 0; i < numLanes; ++i)
            kit[i] = std::clamp (k[i], 0, numKits - 1);
        dirty = true;
    }

    // All knobs 0..1, 0.5 = kit spec.
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
        dirty = true;
    }

    void process (float laneOut[numLanes])
    {
        if (dirty)
        {
            for (int i = 0; i < numLanes; ++i)
                refresh (i);
            dirty = false;
        }
        for (int i = 0; i < numLanes; ++i)
            laneOut[i] = render (i);
    }

    float envelope (int lane) const { return st[lane].env; }
    int samplesSinceTrigger (int lane) const { return st[lane].age; }

private:
    enum Mode : uint8_t { BP, HP };

    struct Spec
    {
        float tailMs;    // body decay
        float snapMs;    // fast transient on top of the tail (0 = none)
        float snapAmt;   // transient height relative to tail
        float fHz;       // SVF cutoff / centre
        float q;
        Mode mode;
        float lpHz;      // tone LP; >= 20k is open
        float noiseMix;  // 0 = oscillators only, 1 = noise only
        float dropSemi;  // pitch knock: start this many semitones up...
        float dropMs;    // ...and fall back over this long
        float sat;       // built-in saturation 0..1
        float gain;
        float click;     // 4 ms noise burst on the attack
        float bits;      // 0 = full; else round (x * bits) / bits after the envelope
        uint8_t nOsc;
        bool square;
        float hz[6];
    };

    // Per-lane coefficients, rebuilt by refresh().
    struct Coef
    {
        float inc[6] {};          // phase increment per sample (cycles)
        float oscNorm = 1.f;
        int nOsc = 0;
        bool square = false;
        float noiseMix = 1.f;
        float dropDepth = 0.f, dropMul = 0.f;
        float a1[2] {}, a2[2] {}, a3[2] {}, k[2] {}; // SVF [normal, accent]
        bool hp = false;
        float lpC = 1.f;
        float clickAmt = 0.f;
        float tailMul = 0.f, snapMul = 0.f, snapAmt = 0.f, snapNorm = 1.f;
        float pre = 1.f, invTanhPre = 1.f;
        float bits = 0.f;
        float gain = 0.f;
    };

    struct State
    {
        float ph[6] {};
        float ic1 = 0.f, ic2 = 0.f, lp = 0.f;
        float tail = 0.f, snap = 0.f, click = 0.f, pitch = 0.f;
        float level = 0.f, env = 0.f;
        bool accent = false;
        int age = 0;
    };

    static constexpr float kTwoPi = 6.2831853f;

    float noise()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) ((int) (rng >> 16) - 32768) * (1.f / 32768.f);
    }

    // Per-sample multiplier that reaches -60 dB after ms.
    float decayMul (float ms) const
    {
        if (ms <= 0.f)
            return 0.f;
        return std::exp (-6.9078f / (ms * 0.001f * (float) sr));
    }

    static float around (float spec, float amt) { return spec * (0.4f + 1.2f * amt); }
    static float octaves (float amt, float span) { return std::pow (2.f, (amt - 0.5f) * span); }

    void refresh (int i)
    {
        if (kit[i] == kitSample || kit[i] < 0 || kit[i] >= numSynthKits)
            return;
        const Spec& s = kSpec[kit[i]][i];
        auto& c = co[i];
        const float fs = (float) sr;
        const float r = std::pow (2.f, tune[i] / 12.f);

        c.nOsc = std::min ((int) s.nOsc, 6);
        c.square = s.square;
        for (int o = 0; o < c.nOsc; ++o)
            c.inc[o] = s.hz[o] * r / fs;
        c.oscNorm = c.nOsc > 0 ? 1.f / std::sqrt ((float) c.nOsc) : 0.f;
        c.noiseMix = c.nOsc > 0 ? s.noiseMix : 1.f;
        c.dropDepth = std::pow (2.f, s.dropSemi / 12.f) - 1.f;
        c.dropMul = decayMul (s.dropMs);

        // TPT SVF (Zavalishin): g = tan(pi f / fs), k = 1/Q. Stable at any f.
        const float f = s.fHz * r * octaves (editHp[i], 4.f);
        for (int a = 0; a < 2; ++a)
        {
            const float fa = std::clamp (f * (a ? 1.15f : 1.f), 20.f, fs * 0.45f);
            const float g = std::tan (3.14159265f * fa / fs);
            c.k[a] = 1.f / std::max (0.3f, s.q);
            c.a1[a] = 1.f / (1.f + g * (g + c.k[a]));
            c.a2[a] = g * c.a1[a];
            c.a3[a] = g * c.a2[a];
        }
        c.hp = s.mode == HP;

        const float lpHz = s.lpHz * octaves (editLp[i], 4.f);
        c.lpC = lpHz >= fs * 0.45f ? 1.f : 1.f - std::exp (-kTwoPi * lpHz / fs);

        // Click knob scales both the transient and the noise burst.
        const float clickScale = 2.f * editClick[i];
        c.clickAmt = s.click * clickScale;
        c.snapAmt = s.snapAmt * clickScale;
        c.snapNorm = 1.f / (1.f + s.snapAmt);
        // Decay knob: x0.35 .. x2.8 of the kit tail.
        c.tailMul = decayMul (s.tailMs * octaves (editDecay[i], 3.f));
        c.snapMul = decayMul (s.snapMs);

        // tanh(x * pre) / tanh(pre): peaks stay put, tails get thicker.
        c.pre = 1.f + 4.f * (s.sat + editDrive[i]);
        c.invTanhPre = 1.f / std::tanh (c.pre);
        c.bits = s.bits;
        c.gain = around (s.gain, editGain[i]);
    }

    void triggerSample (int lane, bool accent)
    {
        auto& s = st[lane];
        auto& p = sp[lane];
        const auto& v = view[lane];
        p.on = false;
        s.tail = 0.f;
        s.snap = 0.f;
        s.env = 0.f;
        if (v.data == nullptr || v.frames < 1)
            return;
        float a = sampStart[lane];
        float b = sampEnd[lane];
        if (b < a)
            std::swap (a, b);
        const int frames = v.frames;
        int i0 = std::clamp ((int) (a * (float) frames), 0, frames - 1);
        int i1 = std::clamp ((int) (b * (float) frames), 0, frames);
        if (i1 <= i0)
            i1 = std::min (frames, i0 + 1);
        p.start = i0;
        p.end = i1;
        p.rev = sampRev[lane];
        p.pos = p.rev ? (float) (i1 - 1) : (float) i0;
        const double fileSr = v.fileRate > 1.0 ? v.fileRate : sr;
        const float ratio = (float) (fileSr / sr) * std::pow (2.f, tune[lane] / 12.f);
        p.inc = (p.rev ? -1.f : 1.f) * std::max (ratio, 1.0e-4f);
        p.level = accent ? 1.f : 0.65f;
        float atk = sampAtk[lane];
        float rel = sampRel[lane];
        const float sum = atk + rel;
        if (sum > 1.f)
        {
            atk /= sum;
            rel /= sum;
        }
        p.atk = atk;
        p.rel = rel;
        p.on = p.end > p.start;
        s.env = p.on ? p.level : 0.f;
    }

    float renderSample (int lane)
    {
        auto& s = st[lane];
        auto& p = sp[lane];
        if (! p.on)
        {
            s.env = 0.f;
            return 0.f;
        }
        const auto& v = view[lane];
        if (v.data == nullptr || v.frames <= 0 || p.end <= p.start)
        {
            p.on = false;
            s.env = 0.f;
            return 0.f;
        }
        const bool forward = p.inc >= 0.f;
        const float pos = p.pos;
        if (forward ? (pos >= (float) p.end) : (pos < (float) p.start))
        {
            p.on = false;
            s.env = 0.f;
            return 0.f;
        }
        ++s.age;
        const int frames = v.frames;
        const int last = std::max (p.start, std::min (p.end, frames) - 1);
        const int i = std::clamp ((int) std::floor (pos), p.start, last);
        const float frac = pos - std::floor (pos);
        const int j = std::clamp (i + 1, p.start, last);
        float y = v.data[i] + frac * (v.data[j] - v.data[i]);

        const float span = std::max (1.0e-4f, std::abs (p.inc));
        const float fromStart = forward ? (pos - (float) p.start) / span
                                         : ((float) last - pos) / span;
        const float fromEnd = forward ? ((float) p.end - pos) / span
                                       : (pos - (float) p.start) / span;
        const float regionN = std::max (1.f, (float) (p.end - p.start) / span);
        const float floorN = 0.001f * (float) sr;
        const float atkN = std::max (floorN, p.atk * regionN);
        const float relN = std::max (floorN, p.rel * regionN);
        float fade = 1.f;
        if (fromStart < atkN)
            fade *= std::max (0.f, fromStart) / atkN;
        if (fromEnd < relN)
            fade *= std::max (0.f, fromEnd) / relN;

        y *= p.level * around (1.f, editGain[lane]) * fade;
        p.pos += p.inc;
        if (forward ? (p.pos >= (float) p.end) : (p.pos < (float) p.start))
            p.on = false;
        s.env = p.on ? std::max (0.01f, std::min (1.f, std::abs (p.level * fade))) : 0.f;
        return y;
    }

    float render (int lane)
    {
        if (kit[lane] == kitSample)
            return renderSample (lane);
        auto& s = st[lane];
        const auto& c = co[lane];
        // Silent lane: skip the work. -80 dB is below anything audible here.
        if (s.tail < 1e-4f && s.snap < 1e-4f)
        {
            s.env = 0.f;
            return 0.f;
        }
        ++s.age;

        float x = 0.f;
        if (c.nOsc > 0)
        {
            const float bend = 1.f + c.dropDepth * s.pitch;
            s.pitch *= c.dropMul;
            float sum = 0.f;
            for (int o = 0; o < c.nOsc; ++o)
            {
                float& p = s.ph[o];
                p += c.inc[o] * bend;
                p -= std::floor (p);
                sum += c.square ? (p < 0.5f ? 1.f : -1.f) : std::sin (kTwoPi * p);
            }
            x = sum * c.oscNorm * (1.f - c.noiseMix);
        }
        if (c.noiseMix > 0.f)
            x += noise() * c.noiseMix;

        const int a = s.accent ? 1 : 0;
        const float v3 = x - s.ic2;
        const float v1 = c.a1[a] * s.ic1 + c.a2[a] * v3;
        const float v2 = s.ic2 + c.a2[a] * s.ic1 + c.a3[a] * v3;
        s.ic1 = 2.f * v1 - s.ic1;
        s.ic2 = 2.f * v2 - s.ic2;
        // BP is k * band so the peak is unity whatever the Q.
        float y = c.hp ? x - c.k[a] * v1 - v2 : c.k[a] * v1;

        s.lp += c.lpC * (y - s.lp);
        y = s.lp;

        y += c.clickAmt * s.click * noise();
        s.click *= clickMul;

        const float amp = s.level * (s.tail + c.snapAmt * s.snap) * c.snapNorm;
        s.tail *= c.tailMul;
        s.snap *= c.snapMul;
        s.env = std::min (amp, 1.f);
        // Gain before tanh so Drive is bounded by 1 / tanh(pre) <= 1.31.
        y *= amp * c.gain;

        if (c.pre > 1.001f)
            y = std::tanh (y * c.pre) * c.invTanhPre;
        if (c.bits > 0.f)
            y = std::round (y * c.bits) / c.bits;
        return y;
    }

    // Inspired-by analog models. Index = Kit enum, then lane
    // (CHH, OHH, Crash, Cym, Shaker, Clave, Cow, Rim).
    // Columns: tailMs snapMs snapAmt | fHz q mode | lpHz | noise | dropSemi dropMs |
    //          sat gain click bits | nOsc square hz[]
    struct SampPlay
    {
        float pos = 0.f;
        float inc = 1.f;
        float level = 0.f;
        float atk = 0.f;
        float rel = 0.f;
        int start = 0;
        int end = 0;
        bool rev = false;
        bool on = false;
    };

    SampleView view[numLanes] {};
    SampPlay sp[numLanes] {};
    float sampStart[numLanes] {};
    float sampEnd[numLanes] {};
    float sampAtk[numLanes] {};
    float sampRel[numLanes] {};
    bool sampRev[numLanes] {};

    static constexpr Spec kSpec[numSynthKits][numLanes] = {
        // 707 — tight, 8-bit PCM edge, a little dark, almost no saturation
        {
            { 35.f, 3.f, 0.5f,    6500.f, 0.9f, HP, 11000.f, 0.6f, 0.f, 0.f,  0.05f, 1.25f, 0.10f, 128.f, 6, true, { 245.f, 353.f, 467.f, 555.f, 722.f, 980.f } },
            { 280.f, 4.f, 0.4f,   6500.f, 0.9f, HP, 11000.f, 0.6f, 0.f, 0.f,  0.05f, 1.25f, 0.08f, 128.f, 6, true, { 245.f, 353.f, 467.f, 555.f, 722.f, 980.f } },
            { 1100.f, 15.f, 0.5f, 4000.f, 0.7f, HP, 10000.f, 0.8f, 0.f, 0.f,  0.f, 1.3f, 0.05f, 128.f, 6, true, { 245.f, 353.f, 467.f, 555.f, 722.f, 980.f } },
            { 900.f, 10.f, 0.4f,  4200.f, 2.0f, BP, 10000.f, 0.35f, 0.f, 0.f, 0.f, 1.35f, 0.05f, 128.f, 6, true, { 320.f, 540.f, 890.f, 1234.f, 1530.f, 2050.f } },
            { 70.f, 10.f, 1.0f,   6500.f, 2.0f, BP, 12000.f, 1.f, 0.f, 0.f,   0.f, 1.9f, 0.f,   128.f, 0, false, {} },
            { 18.f, 0.f, 0.f,     300.f, 0.7f, HP, 12000.f, 0.f, 0.f, 0.f,    0.1f, 0.66f, 0.05f, 128.f, 1, false, { 2400.f } },
            { 130.f, 8.f, 0.8f,   1100.f, 1.0f, BP, 9000.f, 0.f, 0.f, 0.f,    0.1f, 0.8f, 0.f,   128.f, 2, true, { 587.f, 845.f } },
            { 12.f, 2.f, 1.0f,    350.f, 0.7f, HP, 12000.f, 0.f, 0.f, 0.f,    0.3f, 0.72f, 0.15f, 128.f, 2, false, { 420.f, 1500.f } },
        },
        // 808 — square-stack metal into resonant BP, clipped clave/rim, two-stage cowbell
        {
            { 60.f, 8.f, 0.6f,    7100.f, 1.0f, BP, 20000.f, 0.f, 0.f, 0.f,   0.2f, 0.51f, 0.10f, 0.f, 6, true, { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f } },
            { 500.f, 10.f, 0.4f,  7100.f, 1.0f, BP, 20000.f, 0.f, 0.f, 0.f,   0.2f, 0.51f, 0.08f, 0.f, 6, true, { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f } },
            { 1400.f, 20.f, 0.8f, 4500.f, 0.6f, BP, 20000.f, 0.f, 0.f, 0.f,   0.15f, 0.45f, 0.06f, 0.f, 6, true, { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f } },
            { 1000.f, 15.f, 0.5f, 3440.f, 1.5f, BP, 20000.f, 0.f, 0.f, 0.f,   0.1f, 0.58f, 0.04f, 0.f, 6, true, { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f } },
            { 30.f, 0.f, 0.f,     5000.f, 0.7f, HP, 20000.f, 1.f, 0.f, 0.f,   0.f, 0.9f, 0.f,   0.f, 0, false, {} },
            { 30.f, 0.f, 0.f,     300.f, 0.7f, HP, 20000.f, 0.f, 0.f, 0.f,    0.3f, 0.57f, 0.05f, 0.f, 1, false, { 2500.f } },
            { 280.f, 25.f, 1.5f,  2640.f, 0.8f, BP, 20000.f, 0.f, 0.f, 0.f,   0.2f, 0.61f, 0.f,   0.f, 2, true, { 540.f, 800.f } },
            { 18.f, 3.f, 1.0f,    600.f, 0.7f, HP, 20000.f, 0.f, 0.f, 0.f,    0.8f, 0.55f, 0.15f, 0.f, 2, false, { 455.f, 1667.f } },
        },
        // 909 — metal + noise hats through HP, 6-bit cymbals, snappy rim
        {
            { 45.f, 4.f, 0.8f,    8000.f, 1.1f, HP, 20000.f, 0.5f, 0.f, 0.f,  0.f, 1.1f, 0.20f, 64.f, 6, true, { 311.f, 450.f, 584.f, 727.f, 918.f, 1150.f } },
            { 380.f, 5.f, 0.5f,   7000.f, 1.1f, HP, 20000.f, 0.5f, 0.f, 0.f,  0.f, 1.05f, 0.12f, 64.f, 6, true, { 311.f, 450.f, 584.f, 727.f, 918.f, 1150.f } },
            { 1600.f, 30.f, 0.6f, 5000.f, 0.7f, HP, 20000.f, 0.75f, 0.f, 0.f, 0.f, 1.0f, 0.10f, 64.f, 6, true, { 311.f, 450.f, 584.f, 727.f, 918.f, 1150.f } },
            { 1300.f, 10.f, 0.4f, 5000.f, 2.5f, BP, 20000.f, 0.2f, 0.f, 0.f,  0.f, 1.3f, 0.05f, 64.f, 6, true, { 320.f, 540.f, 890.f, 1234.f, 1530.f, 2050.f } },
            { 60.f, 0.f, 0.f,     7500.f, 1.2f, BP, 20000.f, 1.f, 0.f, 0.f,   0.f, 1.25f, 0.f,   0.f, 0, false, {} },
            { 22.f, 0.f, 0.f,     400.f, 0.7f, HP, 20000.f, 0.f, 7.f, 4.f,    0.2f, 0.58f, 0.10f, 0.f, 1, false, { 2700.f } },
            { 200.f, 15.f, 1.0f,  1500.f, 1.2f, BP, 20000.f, 0.f, 0.f, 0.f,   0.3f, 0.68f, 0.f,   0.f, 2, true, { 555.f, 835.f } },
            { 14.f, 2.f, 1.5f,    500.f, 1.5f, HP, 20000.f, 0.15f, 5.f, 3.f,  0.9f, 0.53f, 0.30f, 0.f, 2, false, { 480.f, 1250.f } },
        },
        // Linn — dark, grainy, woody sine cow/rim with pitch knock
        {
            { 55.f, 6.f, 0.5f,    5000.f, 0.8f, HP, 9000.f, 0.85f, 0.f, 0.f,  0.1f, 1.08f, 0.10f, 64.f, 6, true, { 245.f, 353.f, 467.f, 555.f, 722.f, 980.f } },
            { 420.f, 8.f, 0.4f,   5000.f, 0.8f, HP, 8500.f, 0.85f, 0.f, 0.f,  0.1f, 1.09f, 0.08f, 64.f, 6, true, { 245.f, 353.f, 467.f, 555.f, 722.f, 980.f } },
            { 1300.f, 20.f, 0.5f, 3500.f, 0.7f, HP, 7500.f, 0.9f, 0.f, 0.f,   0.1f, 0.95f, 0.05f, 64.f, 6, true, { 245.f, 353.f, 467.f, 555.f, 722.f, 980.f } },
            { 1000.f, 12.f, 0.4f, 3200.f, 1.8f, BP, 7000.f, 0.3f, 0.f, 0.f,   0.1f, 1.0f, 0.04f, 64.f, 6, true, { 320.f, 540.f, 890.f, 1234.f, 1530.f, 2050.f } },
            { 80.f, 25.f, 1.2f,   5500.f, 1.5f, BP, 9000.f, 1.f, 0.f, 0.f,    0.f, 1.65f, 0.f,   48.f, 0, false, {} },
            { 28.f, 0.f, 0.f,     250.f, 0.7f, HP, 8000.f, 0.f, 5.f, 6.f,     0.15f, 0.63f, 0.05f, 96.f, 1, false, { 2100.f } },
            { 160.f, 10.f, 0.6f,  200.f, 0.7f, HP, 6000.f, 0.f, 3.f, 10.f,    0.25f, 0.59f, 0.f,   64.f, 2, false, { 480.f, 720.f } },
            { 25.f, 3.f, 1.2f,    250.f, 0.7f, HP, 7000.f, 0.1f, 6.f, 4.f,    0.5f, 0.63f, 0.20f, 64.f, 2, false, { 340.f, 1180.f } },
        },
    };

    double sr = 44100.0;
    uint32_t rng = 1;
    bool dirty = true;
    float clickMul = 0.f;
    int kit[numLanes] { kit808, kit808, kit808, kit808, kit808, kit808, kit808, kit808 };
    Coef co[numLanes] {};
    State st[numLanes] {};
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
