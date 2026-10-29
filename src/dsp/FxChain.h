#pragma once

#include "EqBiquad.h"
#include "Mixer.h"
#include "../parameters/ParameterIDs.h"

#include <JuceHeader.h>
#include <cmath>

namespace hkr
{

struct FxChain
{
    void prepare (double sampleRate, int samplesPerBlock, int numChannels)
    {
        sr = sampleRate > 1.0 ? sampleRate : 44100.0;
        const int ch = juce::jmax (1, numChannels);
        const int n = juce::jmax (samplesPerBlock, 64);
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) n, (juce::uint32) ch };
        juce::dsp::ProcessSpec mono { sr, (juce::uint32) n, 1 };

        chorusL.prepare (mono);
        chorusR.prepare (mono);
        flanger.prepare (spec);
        phaserL.prepare (mono);
        phaserR.prepare (mono);
        reverb.prepare (spec);
        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) std::ceil (sr * 2.0) + 8);
        delay.reset();
        preDelay.prepare (spec);
        preDelay.setMaximumDelayInSamples ((int) std::ceil (sr * 0.1) + 8);
        preDelay.reset();
        lpL.reset();
        lpR.reset();
        prepared = true;
    }

    void process (juce::AudioBuffer<float>& buffer, float bpm,
                  juce::AudioProcessorValueTreeState& apvts, std::uint64_t packedOrder)
    {
        if (! prepared || buffer.getNumSamples() <= 0)
            return;

        int order[fxCount];
        int n = unpackFxOrder (packedOrder, order);
        bool used[fxCount] {};
        auto on = [&] (int t) -> bool
        {
            auto* p = apvts.getRawParameterValue (ParamID::fxOnIds[t]);
            return p != nullptr && p->load() >= 0.5f;
        };
        for (int i = 0; i < n; ++i)
        {
            used[order[i]] = true;
            if (on (order[i]))
                processOne (order[i], buffer, bpm, apvts);
        }
        for (int t = 0; t < fxCount; ++t)
            if (! used[t] && on (t))
                processOne (t, buffer, bpm, apvts);
    }

private:
    float p (juce::AudioProcessorValueTreeState& apvts, const char* id) const
    {
        auto* v = apvts.getRawParameterValue (id);
        return v != nullptr ? v->load() : 0.f;
    }

    void setChorus (juce::dsp::Chorus<float>& c, float rate, float depth, float fb, float centreMs)
    {
        c.setRate (rate);
        c.setDepth (depth);
        c.setFeedback (fb);
        c.setMix (1.f);
        c.setCentreDelay (centreMs);
    }

    void setPhaser (juce::dsp::Phaser<float>& ph, float rate, float depth, float fb, float hz)
    {
        ph.setRate (rate);
        ph.setDepth (depth);
        ph.setFeedback (fb);
        ph.setMix (1.f);
        ph.setCentreFrequency (hz);
    }

    void processOne (int type, juce::AudioBuffer<float>& buffer, float bpm,
                     juce::AudioProcessorValueTreeState& apvts)
    {
        auto block = juce::dsp::AudioBlock<float> (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        switch (type)
        {
            case 0:
            {
                const float rate = 0.05f + p (apvts, ParamID::fxChoRate) * 7.95f;
                const float depth = p (apvts, ParamID::fxChoDepth);
                const float fb = p (apvts, ParamID::fxChoFb) * 0.9f;
                const float width = p (apvts, ParamID::fxChoWidth);
                setChorus (chorusL, rate, depth, fb, 7.5f);
                setChorus (chorusR, rate, depth, fb, 7.5f + width * 10.f);
                auto left = block.getSingleChannelBlock (0);
                juce::dsp::ProcessContextReplacing<float> ctxL (left);
                chorusL.process (ctxL);
                if (buffer.getNumChannels() > 1)
                {
                    auto right = block.getSingleChannelBlock (1);
                    juce::dsp::ProcessContextReplacing<float> ctxR (right);
                    chorusR.process (ctxR);
                }
                break;
            }
            case 1:
                processDelay (buffer, bpm, apvts);
                break;
            case 2:
                flanger.setRate (0.05f + p (apvts, ParamID::fxFlaRate) * 4.95f);
                flanger.setDepth (p (apvts, ParamID::fxFlaDepth));
                flanger.setFeedback (p (apvts, ParamID::fxFlaFb) * 0.9f);
                flanger.setMix (1.f);
                flanger.setCentreDelay (p (apvts, ParamID::fxFlaDelay));
                flanger.process (ctx);
                break;
            case 3:
            {
                const float rate = 0.05f + p (apvts, ParamID::fxPhaRate) * 7.95f;
                const float depth = p (apvts, ParamID::fxPhaDepth);
                const float fb = p (apvts, ParamID::fxPhaFb) * 0.85f;
                const float centre = p (apvts, ParamID::fxPhaCentre);
                const float spread = p (apvts, ParamID::fxPhaSpread);
                const float den = 1.f + spread;
                float hzL = centre / den;
                float hzR = centre * den;
                hzL = juce::jlimit (200.f, 4000.f, hzL);
                hzR = juce::jlimit (200.f, 4000.f, hzR);
                setPhaser (phaserL, rate, depth, fb, hzL);
                setPhaser (phaserR, rate, depth, fb, hzR);
                auto left = block.getSingleChannelBlock (0);
                juce::dsp::ProcessContextReplacing<float> ctxL (left);
                phaserL.process (ctxL);
                if (buffer.getNumChannels() > 1)
                {
                    auto right = block.getSingleChannelBlock (1);
                    juce::dsp::ProcessContextReplacing<float> ctxR (right);
                    phaserR.process (ctxR);
                }
                break;
            }
            case 4:
            {
                juce::dsp::Reverb::Parameters rp;
                rp.roomSize = p (apvts, ParamID::fxRevSize);
                rp.damping = p (apvts, ParamID::fxRevDamp);
                rp.wetLevel = 1.f;
                rp.dryLevel = 0.f;
                rp.width = p (apvts, ParamID::fxRevWidth);
                rp.freezeMode = 0.f;
                reverb.setParameters (rp);
                processPredelay (buffer, p (apvts, ParamID::fxRevPre));
                reverb.process (ctx);
                break;
            }
            default:
                break;
        }
    }

    void processPredelay (juce::AudioBuffer<float>& buffer, float ms)
    {
        const float maxS = (float) preDelay.getMaximumDelayInSamples() - 1;
        preDelay.setDelay (fxPreDelaySamples (ms, (float) sr, maxS));
        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < chs; ++ch)
            {
                const float in = buffer.getSample (ch, i);
                const float d = preDelay.popSample (ch);
                buffer.setSample (ch, i, d);
                preDelay.pushSample (ch, in);
            }
        }
    }

    void processDelay (juce::AudioBuffer<float>& buffer, float bpm,
                       juce::AudioProcessorValueTreeState& apvts)
    {
        const bool sync = p (apvts, ParamID::fxDlySync) >= 0.5f;
        float delaySec = 0.01f + p (apvts, ParamID::fxDlyTime) * 1.99f;
        if (sync)
        {
            const int div = juce::jlimit (0, 3, juce::roundToInt (p (apvts, ParamID::fxDlyDiv)));
            const float beats = div == 0 ? 0.25f : div == 1 ? 0.5f : div == 2 ? 1.f : 2.f;
            const float b = bpm > 1.f ? bpm : 120.f;
            delaySec = beats * 60.f / b;
        }
        const float delaySamp = juce::jlimit (1.f, (float) delay.getMaximumDelayInSamples() - 1,
                                              delaySec * (float) sr);
        delay.setDelay (delaySamp);

        const float fb = p (apvts, ParamID::fxDlyFb) * 0.92f;
        const float cut = 200.f + p (apvts, ParamID::fxDlyCut) * 9800.f;
        const bool ping = p (apvts, ParamID::fxDlyPing) >= 0.5f && buffer.getNumChannels() > 1;
        const Biquad coef = eqBiquad (eqTypeLP, (float) sr, cut, 0.7f, 0.f);

        const int n = buffer.getNumSamples();
        const int chs = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            if (ping)
            {
                const float inL = buffer.getSample (0, i);
                const float inR = buffer.getSample (1, i);
                const float dL = lpL.process (delay.popSample (0), coef);
                const float dR = lpR.process (delay.popSample (1), coef);
                buffer.setSample (0, i, dL);
                buffer.setSample (1, i, dR);
                float pushL = 0.f, pushR = 0.f;
                delayPingPush (true, inL, inR, dL, dR, fb, pushL, pushR);
                delay.pushSample (0, pushL);
                delay.pushSample (1, pushR);
            }
            else
            {
                for (int ch = 0; ch < chs; ++ch)
                {
                    const float in = buffer.getSample (ch, i);
                    Df1& lp = ch == 0 ? lpL : lpR;
                    const float d = lp.process (delay.popSample (ch), coef);
                    buffer.setSample (ch, i, d);
                    delay.pushSample (ch, in + d * fb);
                }
            }
        }
    }

    juce::dsp::Chorus<float> chorusL;
    juce::dsp::Chorus<float> chorusR;
    juce::dsp::Chorus<float> flanger;
    juce::dsp::Phaser<float> phaserL;
    juce::dsp::Phaser<float> phaserR;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 9600 };
    Df1 lpL, lpR;
    double sr = 44100.0;
    bool prepared = false;
};

} // namespace hkr
