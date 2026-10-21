#pragma once

#include "dsp/EqAnalyser.h"
#include "dsp/FxChain.h"
#include "dsp/Meter.h"
#include "dsp/Mixer.h"
#include "dsp/ParametricEq.h"
#include "dsp/Sequencer.h"
#include "dsp/Voices.h"
#include "parameters/ParameterLayout.h"

#include <JuceHeader.h>
#include <atomic>
#include <cstdint>
#include <vector>

class HKR5AudioProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    HKR5AudioProcessor();
    ~HKR5AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "5HKR"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    void selectSlot (int bank, int pat);
    int currentBank() const { return curBank.load (std::memory_order_relaxed); }
    int currentPattern() const { return curPattern.load (std::memory_order_relaxed); }
    int playhead() const { return playheadUi.load (std::memory_order_relaxed); }
    bool held() const { return heldKey.load (std::memory_order_relaxed) >= 0; }

    hkr::SlotData currentSlot() const;
    void setStep (int lane, int index, int value);
    int cycleStep (int lane, int index);
    void audition (int lane, bool accent);
    void setLoop (int lane, int length);
    void toggleLoopEnd (int lane, int stepIndex);
    void setPatLen (int length);
    void clearPattern();
    void randomizePattern();
    void renderLaneShot (int lane, float* dest, int n, double sampleRate);
    float voiceEnv (int lane) const;
    float voiceAgeSec (int lane) const;
    float lanePeak (int lane) const;

    // Message thread. Mono, trimmed to 4 seconds. Empty file leaves the slot as it was.
    bool loadLaneSample (int lane, const juce::File& file);
    void clearLaneSamples();
    bool laneHasSample (int lane) const;
    juce::String laneSampleName (int lane) const;
    double laneSampleRate (int lane) const;
    void copyLaneSample (int lane, std::vector<float>& dest) const;

    hkr::OutputMeter& outputMeter() { return meter; }
    hkr::EqAnalyser& eqAnalyser() { return analyser; }
    hkr::EqAnalyser& scAnalyser() { return scSpec; }

    void getFxOrder (int* types, int& n) const;
    void setFxEnabled (int type, bool on);
    void moveFx (int fromType, int beforeType);

    juce::File patchesDir() const;
    juce::File banksDir() const;
    juce::StringArray patchNames() const;
    juce::StringArray bankNames() const;
    juce::String patchName() const;
    juce::String bankName() const;
    void initPatch();
    void initBank();
    bool loadPatchByName (const juce::String& name);
    bool loadBankByName (const juce::String& name);
    bool savePatch();
    bool saveBank();
    bool savePatchAs (const juce::String& name);
    bool saveBankAs (const juce::String& name);
    bool exportPatch (const juce::File& dest);
    bool exportBank (const juce::File& dest);
    bool importPatch (const juce::File& src);
    bool importBank (const juce::File& src);

    float uiScale = 0.85f;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    float raw (const char* id) const;
    void applyVoiceParams (hkr::Voices& v) const;
    float tempoBpm() const;
    bool handleKeyTriggers (const juce::MidiBuffer& midi);
    void copySlots (hkr::SlotData dest[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank]) const;
    void applySlots (const hkr::SlotData src[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank]);
    void snapSlot (int bank, int pat, hkr::SlotData& out) const;
    void writeBanksToState();
    void loadBanksFromState();
    void publishFxOrder (const int* types, int n);
    void writeFxOrderToState();
    void loadFxOrderFromState();
    bool loadPatchFile (const juce::File& src, const juce::String& shownName);
    bool loadBankFile (const juce::File& src, const juce::String& shownName);
    void publishLaneSample (int lane, std::vector<float> pcm, float rate, juce::String name, bool writeState);
    void clearLaneSampleAudio (int lane);
    void writeLaneSamplesToState();
    void loadLaneSamplesFromState();
    void pushSampleViews (hkr::Voices& v) const;

    // ponytail: two buffers per lane. Message thread writes the inactive side,
    // then flips `active`. Audio only reads. Ceiling: a second publish of the
    // same lane in the same audio block waits out that block (about one buffer).
    struct LaneSamp
    {
        std::vector<float> buf[2];
        std::atomic<int> frames[2] { 0, 0 };
        std::atomic<int> active { 0 };
        std::atomic<float> fileRate { 44100.f };
        juce::String name;
        std::uint32_t leftEpoch[2] {};
    };
    LaneSamp laneSamp[hkr::numLanes];
    std::atomic<std::uint32_t> audioEpoch { 0 };

    hkr::Sequencer sequencer;
    hkr::Voices voices;
    hkr::LaneMixer laneMixer;
    hkr::FxChain fx;
    hkr::ParametricEq postEq;
    hkr::EqAnalyser analyser;
    hkr::EqAnalyser scSpec;
    hkr::Ducker ducker;
    hkr::ScFilter scFiltL, scFiltR;
    hkr::OutputMeter meter;
    juce::AudioBuffer<float> sendBuf;
    std::atomic<std::uint64_t> fxOrder { 0 };

    std::atomic<uint8_t> slotLen[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank] {};
    std::atomic<uint8_t> slotLoop[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank][hkr::numLanes] {};
    std::atomic<uint32_t> slotWords[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank][hkr::numLanes][hkr::SlotData::wordsPerLane] {};

    std::atomic<int> curBank { 0 };
    std::atomic<int> curPattern { 0 };
    std::atomic<int> heldKey { -1 };
    std::atomic<int> playheadUi { -1 };
    std::atomic<int> pendingAudition { -1 }; // packed (lane << 1) | accent, -1 idle
    std::atomic<float> uiEnv[hkr::numLanes] {};
    std::atomic<float> uiAgeSec[hkr::numLanes] {};
    hkr::LaneMeters laneMeters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HKR5AudioProcessor)
};
