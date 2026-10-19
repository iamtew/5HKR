#pragma once

#include "ParameterIDs.h"

#include <JuceHeader.h>

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (NormalisableRange<float> range)
    {
        return AudioParameterFloatAttributes()
            .withStringFromValueFunction ([range] (float v, int)
            {
                return String (roundToInt (range.convertTo0to1 (v) * 100.f)) + "%";
            })
            .withValueFromStringFunction ([range] (const String& t)
            {
                return range.convertFrom0to1 (jlimit (0.f, 1.f, t.getFloatValue() / 100.f));
            });
    };

    auto unit = NormalisableRange<float> (0.f, 1.f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::volume, 1 }, "Volume", unit, 0.7f, pct (unit)));

    auto bpmR = NormalisableRange<float> (40.f, 240.f, 0.1f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::seqTempo, 1 }, "Tempo", bpmR, 120.f));

    StringArray banks;
    for (int i = 0; i < hkr::Sequencer::numBanks; ++i)
        banks.add ("Bank " + String (i + 1));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::seqBank, 1 }, "Bank", banks, 0));

    StringArray pats;
    for (int i = 0; i < hkr::Sequencer::patternsPerBank; ++i)
        pats.add ("Pat " + String (i + 1));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::seqPattern, 1 }, "Pattern", pats, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::scOn, 1 }, "SC On", false));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::scListen, 1 }, "SC Listen", false));
    auto hpR = NormalisableRange<float> (20.f, 2000.f, 0.1f, 0.4f);
    auto lpR = NormalisableRange<float> (200.f, 20000.f, 1.f, 0.4f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scHp, 1 }, "SC HP", hpR, 20.f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scLp, 1 }, "SC LP", lpR, 20000.f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scThresh, 1 }, "SC Thresh", unit, 0.25f, pct (unit)));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scAmount, 1 }, "SC Amount", unit, 0.7f, pct (unit)));
    auto msA = NormalisableRange<float> (0.1f, 50.f, 0.01f, 0.4f);
    auto msR = NormalisableRange<float> (10.f, 800.f, 0.1f, 0.4f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scAttack, 1 }, "SC Attack", msA, 5.f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scRelease, 1 }, "SC Release", msR, 120.f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::scMix, 1 }, "SC Mix", unit, 1.f, pct (unit)));

    auto swingR = NormalisableRange<float> (0.5f, 0.75f, 0.001f);
    auto tuneR = NormalisableRange<float> (-12.f, 12.f, 0.01f);
    StringArray kits;
    for (int i = 0; i < hkr::numKits; ++i)
        kits.add (hkr::kitNames[i]);
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ParamID::laneOn[i], 1 },
            String (hkr::laneNames[i]) + " On", true));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::laneKit[i], 1 },
            String (hkr::laneNames[i]) + " Kit", kits, hkr::kit808));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSwing[i], 1 },
            String (hkr::laneNames[i]) + " Swing", swingR, 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneTune[i], 1 },
            String (hkr::laneNames[i]) + " Tune", tuneR, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSend[i], 1 },
            String (hkr::laneNames[i]) + " FX", unit, 0.f, pct (unit)));
    }

    auto addOn = [&] (const char* id, const char* name)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, false));
    };
    auto addUnit = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, unit, def, pct (unit)));
    };

    addOn (ParamID::fxOnIds[0], "Chorus On");
    addUnit (ParamID::fxChoRate, "Chorus Rate", 0.25f);
    addUnit (ParamID::fxChoDepth, "Chorus Depth", 0.4f);
    addUnit (ParamID::fxChoMix, "Chorus Mix", 0.4f);
    addUnit (ParamID::fxChoFb, "Chorus Fb", 0.1f);

    addOn (ParamID::fxOnIds[1], "Delay On");
    addUnit (ParamID::fxDlyTime, "Delay Time", 0.35f);
    addUnit (ParamID::fxDlyFb, "Delay Fb", 0.35f);
    addUnit (ParamID::fxDlyMix, "Delay Mix", 0.3f);
    addUnit (ParamID::fxDlyCut, "Delay Cut", 0.7f);
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::fxDlySync, 1 }, "Delay Sync", true));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::fxDlyDiv, 1 }, "Delay Div",
        StringArray { "1/16", "1/8", "1/4", "1/2" }, 1));

    addOn (ParamID::fxOnIds[2], "Flanger On");
    addUnit (ParamID::fxFlaRate, "Flanger Rate", 0.2f);
    addUnit (ParamID::fxFlaDepth, "Flanger Depth", 0.5f);
    addUnit (ParamID::fxFlaFb, "Flanger Fb", 0.45f);
    addUnit (ParamID::fxFlaMix, "Flanger Mix", 0.4f);

    addOn (ParamID::fxOnIds[3], "Phaser On");
    addUnit (ParamID::fxPhaRate, "Phaser Rate", 0.2f);
    addUnit (ParamID::fxPhaDepth, "Phaser Depth", 0.6f);
    addUnit (ParamID::fxPhaFb, "Phaser Fb", 0.3f);
    addUnit (ParamID::fxPhaMix, "Phaser Mix", 0.5f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxPhaCentre, 1 }, "Phaser Centre",
        NormalisableRange<float> (200.f, 4000.f, 0.01f, 0.3f), 1300.f));

    addOn (ParamID::fxOnIds[4], "Reverb On");
    addUnit (ParamID::fxRevSize, "Reverb Size", 0.45f);
    addUnit (ParamID::fxRevDamp, "Reverb Damp", 0.4f);
    addUnit (ParamID::fxRevMix, "Reverb Mix", 0.25f);
    addUnit (ParamID::fxRevWidth, "Reverb Width", 0.8f);

    const float eqHzDef[ParamID::eqBandCount] = { 80.f, 250.f, 1000.f, 4000.f, 10000.f };
    auto eqHz = NormalisableRange<float> (20.f, 20000.f, 0.01f, 0.3f);
    auto eqQ = NormalisableRange<float> (0.1f, 10.f, 0.01f, 0.4f);
    auto eqGain = NormalisableRange<float> (-12.f, 12.f, 0.01f);
    auto eqTypes = StringArray { "Off", "Peak", "Low S", "High S", "HP", "LP" };
    addOn (ParamID::postEqOn, "Post EQ On");
    for (int b = 0; b < ParamID::eqBandCount; ++b)
    {
        const auto n = String (b + 1);
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::postEqType[b], 1 }, "EQ " + n + " Type", eqTypes, 1));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::postEqFreq[b], 1 }, "EQ " + n + " Freq", eqHz, eqHzDef[b]));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::postEqGain[b], 1 }, "EQ " + n + " Gain", eqGain, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::postEqQ[b], 1 }, "EQ " + n + " Q", eqQ, 0.7f));
    }

    return layout;
}
