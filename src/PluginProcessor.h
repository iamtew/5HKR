#pragma once

#include "dsp/Meter.h"
#include "dsp/Sequencer.h"
#include "dsp/Voices.h"
#include "parameters/ParameterLayout.h"

#include <JuceHeader.h>
#include <atomic>

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
    void setLoop (int lane, int length);
    void setPatLen (int length);

    hkr::OutputMeter& outputMeter() { return meter; }

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
    float tempoBpm() const;
    bool handleKeyTriggers (const juce::MidiBuffer& midi);
    void copySlots (hkr::SlotData dest[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank]) const;
    void applySlots (const hkr::SlotData src[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank]);
    void snapSlot (int bank, int pat, hkr::SlotData& out) const;
    void writeBanksToState();
    void loadBanksFromState();
    bool loadPatchFile (const juce::File& src, const juce::String& shownName);
    bool loadBankFile (const juce::File& src, const juce::String& shownName);

    hkr::Sequencer sequencer;
    hkr::Voices voices;
    hkr::Ducker ducker;
    hkr::OutputMeter meter;

    std::atomic<uint8_t> slotLen[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank] {};
    std::atomic<uint8_t> slotLoop[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank][hkr::numLanes] {};
    std::atomic<uint32_t> slotWords[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank][hkr::numLanes][hkr::SlotData::wordsPerLane] {};

    std::atomic<int> curBank { 0 };
    std::atomic<int> curPattern { 0 };
    std::atomic<int> heldKey { -1 };
    std::atomic<int> playheadUi { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HKR5AudioProcessor)
};
