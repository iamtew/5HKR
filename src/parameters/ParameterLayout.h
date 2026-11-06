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
    auto probR = NormalisableRange<float> (0.f, 100.f, 1.f);
    auto tuneR = NormalisableRange<float> (-12.f, 12.f, 0.01f);
    auto mixDb = NormalisableRange<float> (-12.f, 12.f, 0.01f);
    auto filtR = NormalisableRange<float> (-1.f, 1.f, 0.001f);
    auto panR = NormalisableRange<float> (-1.f, 1.f, 0.001f);
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
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneProb[i], 1 },
            String (hkr::laneNames[i]) + " Prob", probR, 100.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneDecay[i], 1 },
            String (hkr::laneNames[i]) + " Decay", unit, 0.5f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneClick[i], 1 },
            String (hkr::laneNames[i]) + " Click", unit, 0.5f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneHp[i], 1 },
            String (hkr::laneNames[i]) + " HPF", unit, 0.5f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneLp[i], 1 },
            String (hkr::laneNames[i]) + " Tone", unit, 0.5f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneGain[i], 1 },
            String (hkr::laneNames[i]) + " Level", unit, 0.5f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSampStart[i], 1 },
            String (hkr::laneNames[i]) + " Start", unit, 0.f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSampEnd[i], 1 },
            String (hkr::laneNames[i]) + " End", unit, 1.f, pct (unit)));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ParamID::laneSampRev[i], 1 },
            String (hkr::laneNames[i]) + " Reverse", false));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSampAtk[i], 1 },
            String (hkr::laneNames[i]) + " Attack", unit, 0.f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneSampRel[i], 1 },
            String (hkr::laneNames[i]) + " Release", unit, 0.f, pct (unit)));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneDrive[i], 1 },
            String (hkr::laneNames[i]) + " Drive", unit, 0.f, pct (unit)));
        const auto n = String (hkr::laneNames[i]);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneMixGain[i], 1 }, n + " Gain", mixDb, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneEqHi[i], 1 }, n + " High", mixDb, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneEqMid[i], 1 }, n + " Mid", mixDb, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneEqLo[i], 1 }, n + " Low", mixDb, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneFilter[i], 1 }, n + " Filter", filtR, 0.f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::lanePan[i], 1 }, n + " Pan", panR, 0.f));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ParamID::laneSolo[i], 1 }, n + " Solo", false));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::laneFader[i], 1 }, n + " Fader", unit, 1.f, pct (unit)));
    }

    auto fromNum = [] (const String& t) { return t.getFloatValue(); };
    auto db1 = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
        .withValueFromStringFunction (fromNum);
    auto hz0 = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })
        .withValueFromStringFunction (fromNum);
    auto ms1 = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })
        .withValueFromStringFunction (fromNum);
    auto ratio1 = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " : 1"; })
        .withValueFromStringFunction (fromNum);
    auto q2 = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })
        .withValueFromStringFunction (fromNum);
    auto signed2 = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })
        .withValueFromStringFunction (fromNum);

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
    addUnit (ParamID::fxChoWidth, "Chorus Width", 0.4f);
    addUnit (ParamID::fxChoFb, "Chorus Fb", 0.1f);
    addUnit (ParamID::fxChoMix, "Chorus Mix", 1.f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxChoCentre, 1 }, "Chorus Centre",
        NormalisableRange<float> (1.f, 20.f, 0.01f), 7.5f, ms1));

    addOn (ParamID::fxOnIds[1], "Delay On");
    addUnit (ParamID::fxDlyTime, "Delay Time", 0.35f);
    addUnit (ParamID::fxDlyFb, "Delay Fb", 0.35f);
    addUnit (ParamID::fxDlyCut, "Delay Cut", 0.7f);
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::fxDlySync, 1 }, "Delay Sync", true));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::fxDlyDiv, 1 }, "Delay Div",
        StringArray { "1/16", "1/8", "1/4", "1/2" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::fxDlyPing, 1 }, "Delay Ping", false));
    addUnit (ParamID::fxDlyMix, "Delay Mix", 1.f);
    addUnit (ParamID::fxDlyHp, "Delay HP", 0.15f);
    addUnit (ParamID::fxDlySpread, "Delay Spread", 0.1f);

    addOn (ParamID::fxOnIds[2], "Flanger On");
    addUnit (ParamID::fxFlaRate, "Flanger Rate", 0.2f);
    addUnit (ParamID::fxFlaDepth, "Flanger Depth", 0.5f);
    addUnit (ParamID::fxFlaFb, "Flanger Fb", 0.45f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxFlaDelay, 1 }, "Flanger Delay",
        NormalisableRange<float> (0.4f, 4.f, 0.01f), 1.6f, ms1));
    addUnit (ParamID::fxFlaMix, "Flanger Mix", 1.f);
    addUnit (ParamID::fxFlaWidth, "Flanger Width", 0.2f);

    addOn (ParamID::fxOnIds[3], "Phaser On");
    addUnit (ParamID::fxPhaRate, "Phaser Rate", 0.2f);
    addUnit (ParamID::fxPhaDepth, "Phaser Depth", 0.6f);
    addUnit (ParamID::fxPhaFb, "Phaser Fb", 0.3f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxPhaCentre, 1 }, "Phaser Centre",
        NormalisableRange<float> (200.f, 4000.f, 0.01f, 0.3f), 1300.f, hz0));
    addUnit (ParamID::fxPhaSpread, "Phaser Spread", 0.3f);
    addUnit (ParamID::fxPhaMix, "Phaser Mix", 1.f);
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::fxPhaStages, 1 }, "Phaser Stages",
        StringArray { "2", "4", "6", "8" }, 2));

    addOn (ParamID::fxOnIds[4], "Reverb On");
    addUnit (ParamID::fxRevSize, "Reverb Size", 0.45f);
    addUnit (ParamID::fxRevDamp, "Reverb Damp", 0.4f);
    addUnit (ParamID::fxRevWidth, "Reverb Width", 0.8f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxRevPre, 1 }, "Reverb Pre",
        NormalisableRange<float> (0.f, 80.f, 0.1f), 0.f, ms1));
    addUnit (ParamID::fxRevMix, "Reverb Mix", 1.f);
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::fxRevFreeze, 1 }, "Reverb Freeze", false));
    addUnit (ParamID::fxRevLo, "Reverb Lo Cut", 0.2f);

    addOn (ParamID::fxOnIds[5], "Comp On");
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpThr, 1 }, "Comp Thresh",
        NormalisableRange<float> (-40.f, 0.f), -12.f, db1));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpRat, 1 }, "Comp Ratio",
        NormalisableRange<float> (1.f, 20.f, 0.1f, 0.4f), 4.f, ratio1));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpAtk, 1 }, "Comp Attack",
        NormalisableRange<float> (0.1f, 100.f, 0.01f, 0.4f), 10.f, ms1));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpRel, 1 }, "Comp Release",
        NormalisableRange<float> (10.f, 500.f, 0.1f, 0.4f), 80.f, ms1));
    addUnit (ParamID::fxCmpMix, "Comp Mix", 1.f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpMake, 1 }, "Comp Makeup",
        NormalisableRange<float> (-6.f, 24.f), 0.f, db1));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpKnee, 1 }, "Comp Knee",
        NormalisableRange<float> (0.f, 12.f), 3.f, db1));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxCmpHpf, 1 }, "Comp HPF",
        NormalisableRange<float> (20.f, 400.f, 0.01f, 0.3f), 80.f, hz0));

    addOn (ParamID::fxOnIds[6], "Dist On");
    addUnit (ParamID::fxDstDrive, "Dist Drive", 0.4f);
    addUnit (ParamID::fxDstMix, "Dist Mix", 0.5f);
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::fxDstType, 1 }, "Dist Type",
        StringArray { "Soft", "Hard", "Fold", "Rectify" }, 0));
    addUnit (ParamID::fxDstTone, "Dist Tone", 0.7f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxDstBias, 1 }, "Dist Bias",
        NormalisableRange<float> (-1.f, 1.f), 0.f, signed2));

    addOn (ParamID::fxOnIds[7], "EQ On");
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxEqTrim, 1 }, "FX EQ Trim",
        NormalisableRange<float> (-12.f, 12.f), 0.f, db1));

    addOn (ParamID::fxOnIds[8], "FX Filter On");
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::fxFltCut, 1 }, "FX Cutoff",
        NormalisableRange<float> (20.f, 12000.f, 0.01f, 0.3f), 2000.f, hz0));
    addUnit (ParamID::fxFltRes, "FX Res", 0.2f);
    addUnit (ParamID::fxFltMix, "FX Filt Mix", 1.f);
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::fxFltType, 1 }, "FX Filt Type",
        StringArray { "Low", "Band", "High" }, 0));
    addUnit (ParamID::fxFltDrive, "FX Filt Drive", 0.f);
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::fxFltSlope, 1 }, "FX Filt Slope",
        StringArray { "12 dB", "24 dB" }, 0));

    const float eqHzDef[ParamID::eqBandCount] = { 80.f, 250.f, 1000.f, 4000.f, 10000.f };
    auto eqHz = NormalisableRange<float> (20.f, 20000.f, 0.01f, 0.3f);
    auto eqQ = NormalisableRange<float> (0.1f, 10.f, 0.01f, 0.4f);
    auto eqGain = NormalisableRange<float> (-12.f, 12.f, 0.01f);
    auto eqTypes = StringArray { "Off", "Peak", "Low S", "High S", "HP", "LP" };
    auto addEqBands = [&] (const char* const* typeIds, const char* const* freqIds,
                           const char* const* gainIds, const char* const* qIds,
                           const char* prefix)
    {
        for (int b = 0; b < ParamID::eqBandCount; ++b)
        {
            const String n = String (prefix) + " " + String (b + 1);
            layout.add (std::make_unique<AudioParameterChoice> (
                ParameterID { typeIds[b], 1 }, n + " Type", eqTypes, 1));
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { freqIds[b], 1 }, n + " Freq", eqHz, eqHzDef[b], hz0));
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { gainIds[b], 1 }, n + " Gain", eqGain, 0.f, db1));
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { qIds[b], 1 }, n + " Q", eqQ, 0.7f, q2));
        }
    };
    addEqBands (ParamID::fxEqType, ParamID::fxEqFreq, ParamID::fxEqGain, ParamID::fxEqQ, "FX EQ");
    addOn (ParamID::postEqOn, "Post EQ On");
    addEqBands (ParamID::postEqType, ParamID::postEqFreq, ParamID::postEqGain, ParamID::postEqQ, "EQ");

    return layout;
}
