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
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ParamID::laneOn[i], 1 },
            String (hkr::laneNames[i]) + " On", true));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSwing[i], 1 },
            String (hkr::laneNames[i]) + " Swing", swingR, 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneTune[i], 1 },
            String (hkr::laneNames[i]) + " Tune", tuneR, 0.f));
    }

    return layout;
}
