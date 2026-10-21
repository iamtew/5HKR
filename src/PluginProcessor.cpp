#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "parameters/Library.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
const juce::Identifier kBanksXml { "BANKS_XML" };
const juce::Identifier kPatchName { "PATCH_NAME" };
const juce::Identifier kBankName { "BANK_NAME" };
const juce::Identifier kFxOrder { "FX_ORDER" };
const juce::Identifier kLaneSamples { "LANE_SAMPLES" };
const juce::Identifier kSampKits { "sampKits" };
constexpr const char* kInitPatch = "Init Patch";
constexpr const char* kInitBank = "Init Bank";
constexpr const char* kPatchExt = ".5hkrp";
constexpr const char* kBankExt = ".5hkrb";

juce::File libraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Stupid Systems LLC")
        .getChildFile ("5HKR");
}

juce::File ensureDir (juce::File dir)
{
    dir.createDirectory();
    return dir;
}

juce::StringArray listStemNames (const juce::File& dir, const char* wildcard)
{
    juce::StringArray names;
    if (! dir.isDirectory())
        return names;
    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, wildcard))
        names.add (entry.getFile().getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::String legalStem (const juce::String& name)
{
    auto stem = juce::File::createLegalFileName (name.trim());
    if (stem.endsWithIgnoreCase (kPatchExt))
        stem = stem.dropLastCharacters ((int) std::strlen (kPatchExt));
    if (stem.endsWithIgnoreCase (kBankExt))
        stem = stem.dropLastCharacters ((int) std::strlen (kBankExt));
    return stem;
}

// laneKit was a 4-choice param (norm = index/3). Sample is the 5th (norm = index/4).
// Old "1" was Linn. Only files from before sampKits need the remap.
void migrateFourKits (juce::XmlElement& xml)
{
    if (xml.hasAttribute (kSampKits))
        return;
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        const juce::String key (ParamID::laneKit[i]);
        if (! xml.hasAttribute (key))
            continue;
        const float norm = (float) xml.getDoubleAttribute (key, 0.0);
        const int index = juce::jlimit (0, hkr::numSynthKits - 1, juce::roundToInt (norm * 3.f));
        xml.setAttribute (key, (double) index / (double) (hkr::numKits - 1));
    }
    xml.setAttribute (kSampKits, 1);
}

void stripPatchExtras (juce::ValueTree& t)
{
    t.removeProperty (kBanksXml, nullptr);
    t.removeProperty (kBankName, nullptr);
}

} // namespace

HKR5AudioProcessor::HKR5AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withOutput (hkr::laneNames[hkr::chh], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::ohh], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::crash], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::cymbal], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::shaker], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::clave], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::cowbell], juce::AudioChannelSet::stereo(), false)
                          .withOutput (hkr::laneNames[hkr::rim], juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int b = 0; b < hkr::Sequencer::numBanks; ++b)
        for (int p = 0; p < hkr::Sequencer::patternsPerBank; ++p)
        {
            hkr::SlotData s;
            hkr::fillFactory (s, b, p);
            slotLen[b][p].store (s.len, std::memory_order_relaxed);
            for (int lane = 0; lane < hkr::numLanes; ++lane)
            {
                slotLoop[b][p][lane].store (s.loop[lane], std::memory_order_relaxed);
                for (int w = 0; w < hkr::SlotData::wordsPerLane; ++w)
                    slotWords[b][p][lane][w].store (s.words[lane][w], std::memory_order_relaxed);
            }
        }

    apvts.state.setProperty (kPatchName, kInitPatch, nullptr);
    apvts.state.setProperty (kBankName, kInitBank, nullptr);
    apvts.state.setProperty (kSampKits, 1, nullptr);
    apvts.addParameterListener (ParamID::seqBank, this);
    apvts.addParameterListener (ParamID::seqPattern, this);
    int order[hkr::fxCount];
    publishFxOrder (order, hkr::fillDefaultFxOrder (order));
    loadBanksFromState();
    loadFxOrderFromState();
}

HKR5AudioProcessor::~HKR5AudioProcessor()
{
    apvts.removeParameterListener (ParamID::seqBank, this);
    apvts.removeParameterListener (ParamID::seqPattern, this);
}

void HKR5AudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == ParamID::seqBank || parameterID == ParamID::seqPattern)
        selectSlot ((int) raw (ParamID::seqBank), (int) raw (ParamID::seqPattern));
}

void HKR5AudioProcessor::selectSlot (int bank, int pat)
{
    curBank.store (juce::jlimit (0, hkr::Sequencer::numBanks - 1, bank), std::memory_order_relaxed);
    curPattern.store (juce::jlimit (0, hkr::Sequencer::patternsPerBank - 1, pat), std::memory_order_relaxed);
}

void HKR5AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sequencer.prepare (sampleRate);
    voices.prepare (sampleRate);
    laneMixer.prepare (sampleRate);
    ducker.prepare (sampleRate);
    scFiltL.prepare (sampleRate);
    scFiltR.prepare (sampleRate);
    // Master mix + FX send stay stereo even when the host enables lane buses.
    const int ch = 2;
    const int n = juce::jmax (samplesPerBlock, 64);
    sendBuf.setSize (ch, n);
    fx.prepare (sampleRate, n, ch);
    postEq.prepare (sampleRate, n, ch);
    analyser.prepare (sampleRate);
    scSpec.prepare (sampleRate);
    meter.prepare (sampleRate);
    laneMeters.prepare (sampleRate);
}

bool HKR5AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    if (! (in.isDisabled() || in == juce::AudioChannelSet::stereo()
           || in == juce::AudioChannelSet::mono()))
        return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& s = layouts.outputBuses.getReference (i);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()
            && s != juce::AudioChannelSet::mono())
            return false;
    }
    for (int i = 1; i < layouts.inputBuses.size(); ++i)
        if (! layouts.inputBuses.getReference (i).isDisabled())
            return false;
    return true;
}

float HKR5AudioProcessor::raw (const char* id) const
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    return 0.f;
}

void HKR5AudioProcessor::pushSampleViews (hkr::Voices& v) const
{
    hkr::Voices::SampleView views[hkr::numLanes];
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        const int side = laneSamp[i].active.load (std::memory_order_acquire);
        const int n = laneSamp[i].frames[side].load (std::memory_order_acquire);
        views[i].data = n > 0 ? laneSamp[i].buf[side].data() : nullptr;
        views[i].frames = n > 0 ? n : 0;
        views[i].fileRate = (double) laneSamp[i].fileRate.load (std::memory_order_relaxed);
    }
    v.setSamples (views);
}

void HKR5AudioProcessor::applyVoiceParams (hkr::Voices& v) const
{
    float tune[hkr::numLanes];
    float decay[hkr::numLanes];
    float click[hkr::numLanes];
    float hp[hkr::numLanes];
    float lp[hkr::numLanes];
    float gain[hkr::numLanes];
    float drive[hkr::numLanes];
    float start[hkr::numLanes];
    float end[hkr::numLanes];
    bool rev[hkr::numLanes];
    int kits[hkr::numLanes];
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        tune[i] = raw (ParamID::laneTune[i]);
        decay[i] = raw (ParamID::laneDecay[i]);
        click[i] = raw (ParamID::laneClick[i]);
        hp[i] = raw (ParamID::laneHp[i]);
        lp[i] = raw (ParamID::laneLp[i]);
        gain[i] = raw (ParamID::laneGain[i]);
        drive[i] = raw (ParamID::laneDrive[i]);
        start[i] = raw (ParamID::laneSampStart[i]);
        end[i] = raw (ParamID::laneSampEnd[i]);
        rev[i] = raw (ParamID::laneSampRev[i]) >= 0.5f;
        kits[i] = (int) raw (ParamID::laneKit[i]);
    }
    v.setTune (tune);
    v.setKit (kits);
    v.setEdit (decay, click, hp, lp, gain, drive);
    v.setSampleRegion (start, end, rev);
    pushSampleViews (v);
}

float HKR5AudioProcessor::voiceEnv (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return 0.f;
    return uiEnv[lane].load (std::memory_order_relaxed);
}

float HKR5AudioProcessor::voiceAgeSec (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return 0.f;
    return uiAgeSec[lane].load (std::memory_order_relaxed);
}

float HKR5AudioProcessor::lanePeak (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return 0.f;
    return laneMeters.last (lane);
}

void HKR5AudioProcessor::renderLaneShot (int lane, float* dest, int n, double sampleRate)
{
    if (dest == nullptr || n <= 0 || lane < 0 || lane >= hkr::numLanes)
        return;
    hkr::Voices v;
    v.prepare (sampleRate);
    applyVoiceParams (v);
    v.trigger (lane, true);
    float lanes[hkr::numLanes];
    for (int i = 0; i < n; ++i)
    {
        v.process (lanes);
        dest[i] = lanes[lane];
    }
}

float HKR5AudioProcessor::tempoBpm() const
{
    if (wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        if (auto* head = getPlayHead())
            if (const auto pos = head->getPosition())
                if (const auto bpm = pos->getBpm())
                    if (*bpm > 1.0)
                        return (float) *bpm;
    return raw (ParamID::seqTempo);
}

void HKR5AudioProcessor::snapSlot (int bank, int pat, hkr::SlotData& out) const
{
    bank = juce::jlimit (0, hkr::Sequencer::numBanks - 1, bank);
    pat = juce::jlimit (0, hkr::Sequencer::patternsPerBank - 1, pat);
    out.len = slotLen[bank][pat].load (std::memory_order_relaxed);
    for (int lane = 0; lane < hkr::numLanes; ++lane)
    {
        out.loop[lane] = slotLoop[bank][pat][lane].load (std::memory_order_relaxed);
        for (int w = 0; w < hkr::SlotData::wordsPerLane; ++w)
            out.words[lane][w] = slotWords[bank][pat][lane][w].load (std::memory_order_relaxed);
    }
}

hkr::SlotData HKR5AudioProcessor::currentSlot() const
{
    hkr::SlotData s;
    snapSlot (currentBank(), currentPattern(), s);
    return s;
}

void HKR5AudioProcessor::setStep (int lane, int index, int value)
{
    const int b = currentBank();
    const int p = currentPattern();
    if (lane < 0 || lane >= hkr::numLanes || index < 0 || index >= hkr::SlotData::maxSteps)
        return;
    const int w = index / 16;
    const int sh = (index % 16) * 2;
    const uint32_t mask = 3u << sh;
    auto& word = slotWords[b][p][lane][w];
    const uint32_t cur = word.load (std::memory_order_relaxed);
    word.store ((cur & ~mask) | (((uint32_t) (value & 3)) << sh), std::memory_order_relaxed);
}

int HKR5AudioProcessor::cycleStep (int lane, int index)
{
    const auto s = currentSlot();
    const int next = (hkr::getStep (s, lane, index) + 1) % 3;
    setStep (lane, index, next);
    return next;
}

void HKR5AudioProcessor::audition (int lane, bool accent)
{
    if (lane < 0 || lane >= hkr::numLanes)
        return;
    pendingAudition.store ((lane << 1) | (accent ? 1 : 0), std::memory_order_relaxed);
}

void HKR5AudioProcessor::setLoop (int lane, int length)
{
    if (lane < 0 || lane >= hkr::numLanes)
        return;
    const int len = hkr::clampLen ((int) currentSlot().len);
    slotLoop[currentBank()][currentPattern()][lane].store (
        (uint8_t) juce::jlimit (1, len, length), std::memory_order_relaxed);
}

void HKR5AudioProcessor::toggleLoopEnd (int lane, int stepIndex)
{
    const auto s = currentSlot();
    const int len = hkr::clampLen ((int) s.len);
    if (lane < 0 || lane >= hkr::numLanes || stepIndex < 0 || stepIndex >= len)
        return;
    const int end = stepIndex + 1;
    const int cur = juce::jlimit (1, len, (int) s.loop[lane]);
    setLoop (lane, cur == end ? len : end);
}

void HKR5AudioProcessor::setPatLen (int length)
{
    hkr::SlotData s = currentSlot();
    hkr::setPatternLen (s, length);
    const int b = currentBank();
    const int p = currentPattern();
    slotLen[b][p].store (s.len, std::memory_order_relaxed);
    for (int lane = 0; lane < hkr::numLanes; ++lane)
    {
        slotLoop[b][p][lane].store (s.loop[lane], std::memory_order_relaxed);
        for (int w = 0; w < hkr::SlotData::wordsPerLane; ++w)
            slotWords[b][p][lane][w].store (s.words[lane][w], std::memory_order_relaxed);
    }
}

void HKR5AudioProcessor::clearPattern()
{
    hkr::SlotData s = currentSlot();
    hkr::clearPatternSteps (s);
    const int b = currentBank();
    const int p = currentPattern();
    for (int lane = 0; lane < hkr::numLanes; ++lane)
        for (int w = 0; w < hkr::SlotData::wordsPerLane; ++w)
            slotWords[b][p][lane][w].store (s.words[lane][w], std::memory_order_relaxed);
}

void HKR5AudioProcessor::randomizePattern()
{
    hkr::SlotData s = currentSlot();
    hkr::randomizePatternSteps (s, (uint32_t) juce::Random::getSystemRandom().nextInt());
    const int b = currentBank();
    const int p = currentPattern();
    for (int lane = 0; lane < hkr::numLanes; ++lane)
        for (int w = 0; w < hkr::SlotData::wordsPerLane; ++w)
            slotWords[b][p][lane][w].store (s.words[lane][w], std::memory_order_relaxed);
}

bool HKR5AudioProcessor::handleKeyTriggers (const juce::MidiBuffer& midi)
{
    int held = heldKey.load (std::memory_order_relaxed);
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && msg.getFloatVelocity() > 0.f)
        {
            int bank = 0, pat = 0;
            if (! hkr::Sequencer::mapKeyToSlot (msg.getNoteNumber(), bank, pat))
                continue;
            held = msg.getNoteNumber();
            selectSlot (bank, pat);
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getFloatVelocity() <= 0.f))
        {
            if (msg.getNoteNumber() == held)
                held = -1;
        }
    }
    heldKey.store (held, std::memory_order_relaxed);
    return held >= 0;
}

void HKR5AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const bool run = handleKeyTriggers (midi);
    midi.clear();

    auto out = getBusBuffer (buffer, false, 0);
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 0 && getChannelCountOfBus (true, 0) > 0)
    {
        auto sc = getBusBuffer (buffer, true, 0);
        if (sc.getNumSamples() > 0 && sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
        }
    }
    out.clear();

    float* laneL[hkr::numLanes] {};
    float* laneR[hkr::numLanes] {};
    const int nOutBuses = getBusCount (false);
    for (int ln = 0; ln < hkr::numLanes; ++ln)
    {
        const int bus = 1 + ln;
        if (bus >= nOutBuses || getChannelCountOfBus (false, bus) <= 0)
            continue;
        auto aux = getBusBuffer (buffer, false, bus);
        aux.clear();
        if (aux.getNumSamples() > 0 && aux.getNumChannels() > 0)
        {
            laneL[ln] = aux.getWritePointer (0);
            laneR[ln] = aux.getNumChannels() > 1 ? aux.getWritePointer (1) : nullptr;
        }
    }

    hkr::SlotData slot;
    snapSlot (currentBank(), currentPattern(), slot);

    float swing[hkr::numLanes];
    float sendAmt[hkr::numLanes];
    bool solo[hkr::numLanes];
    uint8_t enabled = 0;
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        swing[i] = raw (ParamID::laneSwing[i]);
        sendAmt[i] = raw (ParamID::laneSend[i]);
        solo[i] = raw (ParamID::laneSolo[i]) >= 0.5f;
        if (raw (ParamID::laneOn[i]) >= 0.5f)
            enabled = (uint8_t) (enabled | (1u << i));
        laneMixer.strip[i].set (raw (ParamID::laneMixGain[i]),
                                raw (ParamID::laneEqLo[i]),
                                raw (ParamID::laneEqMid[i]),
                                raw (ParamID::laneEqHi[i]),
                                raw (ParamID::laneFilter[i]),
                                raw (ParamID::laneFader[i]),
                                raw (ParamID::lanePan[i]));
    }
    const std::uint8_t soloMask = hkr::mixerSoloMask (solo);
    applyVoiceParams (voices);

    hkr::Hit hits[64];
    const int n = out.getNumSamples();
    const int nh = sequencer.advance (slot, n, tempoBpm(), run, swing, enabled, hits, 64);
    playheadUi.store (sequencer.playhead(), std::memory_order_relaxed);

    // ponytail: grow only if host exceeds prepare size. Cap at 2 — FX is master-only.
    const int mixCh = juce::jlimit (1, 2, out.getNumChannels());
    if (sendBuf.getNumSamples() < n || sendBuf.getNumChannels() < mixCh)
        sendBuf.setSize (juce::jmax (mixCh, sendBuf.getNumChannels()),
                         juce::jmax (n, sendBuf.getNumSamples()), false, false, true);
    sendBuf.clear();

    float* l = out.getWritePointer (0);
    float* r = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    float* sL = sendBuf.getWritePointer (0);
    float* sR = sendBuf.getNumChannels() > 1 ? sendBuf.getWritePointer (1) : nullptr;

    const float vol = raw (ParamID::volume);
    const bool haveSc = scL != nullptr;
    const bool scOn = raw (ParamID::scOn) >= 0.5f && haveSc;
    const bool scListen = raw (ParamID::scListen) >= 0.5f && haveSc;
    const float thr = raw (ParamID::scThresh);
    const float amt = raw (ParamID::scAmount);
    const float atk = raw (ParamID::scAttack);
    const float rel = raw (ParamID::scRelease);
    const float mix = raw (ParamID::scMix);
    const float hpHz = raw (ParamID::scHp);
    const float lpHz = raw (ParamID::scLp);

    float lanes[hkr::numLanes];
    float peak[hkr::numLanes] {};
    const int packed = pendingAudition.exchange (-1, std::memory_order_relaxed);
    if (packed >= 0)
        voices.trigger (packed >> 1, (packed & 1) != 0);
    for (int i = 0; i < n; ++i)
    {
        for (int h = 0; h < nh; ++h)
            if (hits[h].offset == i)
                voices.trigger (hits[h].lane, hits[h].accent);
        voices.process (lanes);
        float dryL = 0.f, dryR = 0.f, sndL = 0.f, sndR = 0.f;
        for (int ln = 0; ln < hkr::numLanes; ++ln)
        {
            float x = laneMixer.strip[ln].process (lanes[ln]);
            if ((soloMask & (std::uint8_t) (1u << ln)) == 0)
                x = 0.f;
            const float ax = std::fabs (x);
            if (ax > peak[ln])
                peak[ln] = ax;
            const float gL = laneMixer.strip[ln].panL;
            const float gR = laneMixer.strip[ln].panR;
            if (laneL[ln] != nullptr)
            {
                const float yL = x * gL;
                const float yR = x * gR;
                if (laneR[ln] != nullptr)
                {
                    laneL[ln][i] = yL;
                    laneR[ln][i] = yR;
                }
                else
                    laneL[ln][i] = 0.5f * (yL + yR);
            }
            const float d = 1.f - sendAmt[ln];
            const float s = sendAmt[ln];
            dryL += x * d * gL;
            dryR += x * d * gR;
            sndL += x * s * gL;
            sndR += x * s * gR;
        }
        dryL *= vol;
        dryR *= vol;
        sndL *= vol;
        sndR *= vol;
        if (r != nullptr)
        {
            l[i] = dryL;
            r[i] = dryR;
        }
        else
            l[i] = 0.5f * (dryL + dryR);
        if (sR != nullptr)
        {
            sL[i] = sndL;
            sR[i] = sndR;
        }
        else
            sL[i] = 0.5f * (sndL + sndR);
    }

    const float invSr = 1.f / (float) (getSampleRate() > 1.0 ? getSampleRate() : 44100.0);
    for (int ln = 0; ln < hkr::numLanes; ++ln)
    {
        uiEnv[ln].store (voices.envelope (ln), std::memory_order_relaxed);
        uiAgeSec[ln].store ((float) voices.samplesSinceTrigger (ln) * invSr,
                            std::memory_order_relaxed);
    }
    laneMeters.pushBlock (peak, n);

    auto sendView = juce::AudioBuffer<float> (sendBuf.getArrayOfWritePointers(),
                                              out.getNumChannels(), n);
    fx.process (sendView, tempoBpm(), apvts, fxOrder.load (std::memory_order_relaxed));
    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.addFrom (ch, 0, sendBuf, ch, 0, n);

    hkr::EqBandParam bands[hkr::eqBandCount];
    hkr::readEqBands (apvts, bands);
    postEq.process (out, hkr::postEqEnabled (apvts), bands);

    // Filter the input key whenever the bus is there so HP/LP show on the spectrum
    // even before duck/Listen. Same samples go to Ducker.
    if (haveSc)
    {
        for (int i = 0; i < n; ++i)
        {
            const float fL = scFiltL.process (scL[i], hpHz, lpHz);
            const float fR = scFiltR.process (scR[i], hpHz, lpHz);
            scSpec.pushSample (0.5f * (fL + fR));
            if (scOn)
            {
                const float scAbs = 0.5f * (std::abs (fL) + std::abs (fR));
                const float g = ducker.gain (scAbs, thr, amt, atk, rel);
                const float w = (1.f - mix) + mix * g;
                l[i] *= w;
                if (r != nullptr)
                    r[i] *= w;
            }
            if (scListen)
            {
                l[i] = fL;
                if (r != nullptr)
                    r[i] = fR;
            }
        }
    }

    analyser.push (out);
    meter.push (out);
    audioEpoch.fetch_add (1, std::memory_order_release);
}

void HKR5AudioProcessor::copySlots (hkr::SlotData dest[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank]) const
{
    for (int b = 0; b < hkr::Sequencer::numBanks; ++b)
        for (int p = 0; p < hkr::Sequencer::patternsPerBank; ++p)
            snapSlot (b, p, dest[b][p]);
}

void HKR5AudioProcessor::applySlots (const hkr::SlotData src[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank])
{
    for (int b = 0; b < hkr::Sequencer::numBanks; ++b)
        for (int p = 0; p < hkr::Sequencer::patternsPerBank; ++p)
        {
            slotLen[b][p].store (src[b][p].len, std::memory_order_relaxed);
            for (int lane = 0; lane < hkr::numLanes; ++lane)
            {
                slotLoop[b][p][lane].store (src[b][p].loop[lane], std::memory_order_relaxed);
                for (int w = 0; w < hkr::SlotData::wordsPerLane; ++w)
                    slotWords[b][p][lane][w].store (src[b][p].words[lane][w], std::memory_order_relaxed);
            }
        }
}

void HKR5AudioProcessor::writeBanksToState()
{
    hkr::SlotData slots[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank];
    copySlots (slots);
    apvts.state.setProperty (kBanksXml, juce::String (hkr::writeBankXml (slots)), nullptr);
}

void HKR5AudioProcessor::loadBanksFromState()
{
    const auto xml = apvts.state.getProperty (kBanksXml).toString();
    if (xml.isEmpty())
        return;
    hkr::SlotData slots[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank];
    if (hkr::readBankXml (xml.toStdString(), slots))
        applySlots (slots);
}

void HKR5AudioProcessor::publishFxOrder (const int* types, int n)
{
    int buf[hkr::fxCount];
    int m = 0;
    if (types == nullptr || n <= 0)
        m = hkr::fillDefaultFxOrder (buf);
    else
    {
        m = n > hkr::fxCount ? hkr::fxCount : n;
        for (int i = 0; i < m; ++i)
            buf[i] = types[i];
        m = hkr::completeFxOrder (buf, m);
    }
    const auto packed = hkr::packFxOrder (buf, m);
    fxOrder.store (packed, std::memory_order_relaxed);
    char str[32];
    hkr::formatFxOrder (packed, str, (int) sizeof (str));
    apvts.state.setProperty (kFxOrder, juce::String (str), nullptr);
}

void HKR5AudioProcessor::writeFxOrderToState()
{
    char buf[32];
    hkr::formatFxOrder (fxOrder.load (std::memory_order_relaxed), buf, (int) sizeof (buf));
    apvts.state.setProperty (kFxOrder, juce::String (buf), nullptr);
}

void HKR5AudioProcessor::loadFxOrderFromState()
{
    const auto s = apvts.state.getProperty (kFxOrder, "").toString();
    int types[hkr::fxCount];
    int n = hkr::parseFxOrder (s.toRawUTF8(), types);
    n = hkr::completeFxOrder (types, n);
    fxOrder.store (hkr::packFxOrder (types, n), std::memory_order_relaxed);
}

void HKR5AudioProcessor::getFxOrder (int* types, int& n) const
{
    n = hkr::unpackFxOrder (fxOrder.load (std::memory_order_relaxed), types);
    n = hkr::completeFxOrder (types, n);
}

void HKR5AudioProcessor::setFxEnabled (int type, bool on)
{
    if (type < 0 || type >= hkr::fxCount)
        return;
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamID::fxOnIds[type])))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (on ? 1.f : 0.f);
        param->endChangeGesture();
    }
}

void HKR5AudioProcessor::moveFx (int fromType, int beforeType)
{
    if (fromType < 0 || fromType >= hkr::fxCount || fromType == beforeType)
        return;
    int types[hkr::fxCount];
    int n = 0;
    getFxOrder (types, n);
    int tmp[hkr::fxCount];
    int m = 0;
    bool inserted = false;
    for (int i = 0; i < n; ++i)
    {
        if (types[i] == fromType)
            continue;
        if (types[i] == beforeType && ! inserted)
        {
            tmp[m++] = fromType;
            inserted = true;
        }
        tmp[m++] = types[i];
    }
    if (! inserted)
        tmp[m++] = fromType;
    publishFxOrder (tmp, m);
}

void HKR5AudioProcessor::clearLaneSampleAudio (int lane)
{
    if (lane < 0 || lane >= hkr::numLanes)
        return;
    laneSamp[lane].frames[0].store (0, std::memory_order_release);
    laneSamp[lane].frames[1].store (0, std::memory_order_release);
    laneSamp[lane].name.clear();
}

void HKR5AudioProcessor::publishLaneSample (int lane, std::vector<float> pcm, float rate,
                                            juce::String name, bool writeState)
{
    if (lane < 0 || lane >= hkr::numLanes)
        return;
    auto& slot = laneSamp[lane];
    const int previous = slot.active.load (std::memory_order_relaxed);
    const int dest = previous == 0 ? 1 : 0;
    // The in-flight block may still be reading `dest` if we left it this epoch.
    const auto now = audioEpoch.load (std::memory_order_acquire);
    if (slot.leftEpoch[dest] != 0 && slot.leftEpoch[dest] == now
        && juce::MessageManager::existsAndIsCurrentThread())
    {
        for (int i = 0; i < 40 && audioEpoch.load (std::memory_order_acquire) == now; ++i)
            juce::Thread::sleep (1);
    }
    const int n = (int) pcm.size();
    slot.buf[dest] = std::move (pcm);
    slot.fileRate.store (rate > 1.f ? rate : 44100.f, std::memory_order_relaxed);
    slot.name = std::move (name);
    slot.frames[dest].store (n, std::memory_order_release);
    slot.leftEpoch[previous] = audioEpoch.load (std::memory_order_relaxed);
    slot.active.store (dest, std::memory_order_release);
    if (writeState)
        writeLaneSamplesToState();
}

void HKR5AudioProcessor::writeLaneSamplesToState()
{
    auto existing = apvts.state.getChildWithName (kLaneSamples);
    if (existing.isValid())
        apvts.state.removeChild (existing, nullptr);
    juce::ValueTree tree (kLaneSamples);
    for (int lane = 0; lane < hkr::numLanes; ++lane)
    {
        const int side = laneSamp[lane].active.load (std::memory_order_acquire);
        const int n = laneSamp[lane].frames[side].load (std::memory_order_acquire);
        if (n <= 0 || laneSamp[lane].buf[side].empty())
            continue;
        juce::ValueTree samp ("SAMP");
        samp.setProperty ("lane", lane, nullptr);
        samp.setProperty ("name", laneSamp[lane].name, nullptr);
        samp.setProperty ("rate", (double) laneSamp[lane].fileRate.load (std::memory_order_relaxed), nullptr);
        juce::MemoryBlock mb ((size_t) n * sizeof (float));
        std::memcpy (mb.getData(), laneSamp[lane].buf[side].data(), mb.getSize());
        samp.setProperty ("pcm", juce::var (mb), nullptr);
        tree.appendChild (samp, nullptr);
    }
    if (tree.getNumChildren() > 0)
        apvts.state.appendChild (tree, nullptr);
}

void HKR5AudioProcessor::loadLaneSamplesFromState()
{
    bool keep[hkr::numLanes] {};
    auto tree = apvts.state.getChildWithName (kLaneSamples);
    if (tree.isValid())
    {
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto samp = tree.getChild (i);
            const int lane = (int) samp.getProperty ("lane", -1);
            if (lane < 0 || lane >= hkr::numLanes)
                continue;
            auto* mb = samp.getProperty ("pcm").getBinaryData();
            if (mb == nullptr || mb->getSize() < sizeof (float))
                continue;
            const int n = (int) (mb->getSize() / sizeof (float));
            std::vector<float> pcm ((size_t) n);
            std::memcpy (pcm.data(), mb->getData(), (size_t) n * sizeof (float));
            const float rate = (float) (double) samp.getProperty ("rate", 44100.0);
            publishLaneSample (lane, std::move (pcm), rate, samp.getProperty ("name").toString(), false);
            keep[lane] = true;
        }
    }
    for (int lane = 0; lane < hkr::numLanes; ++lane)
        if (! keep[lane])
            clearLaneSampleAudio (lane);
}

void HKR5AudioProcessor::clearLaneSamples()
{
    for (int lane = 0; lane < hkr::numLanes; ++lane)
        clearLaneSampleAudio (lane);
    auto existing = apvts.state.getChildWithName (kLaneSamples);
    if (existing.isValid())
        apvts.state.removeChild (existing, nullptr);
}

bool HKR5AudioProcessor::loadLaneSample (int lane, const juce::File& file)
{
    if (lane < 0 || lane >= hkr::numLanes || ! file.existsAsFile())
        return false;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate < 1.0 || reader->lengthInSamples <= 0)
        return false;
    const auto cap64 = (juce::int64) std::llround (reader->sampleRate * 4.0);
    const int n = (int) std::min (reader->lengthInSamples, std::max<juce::int64> (1, cap64));
    const int chs = std::max (1, (int) reader->numChannels);
    juce::AudioBuffer<float> tmp (chs, n);
    if (! reader->read (&tmp, 0, n, 0, true, true))
        return false;
    std::vector<float> mono ((size_t) n);
    const float inv = 1.f / (float) chs;
    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        for (int c = 0; c < chs; ++c)
            s += tmp.getSample (c, i);
        mono[(size_t) i] = s * inv;
    }
    publishLaneSample (lane, std::move (mono), (float) reader->sampleRate,
                       file.getFileNameWithoutExtension(), true);
    return true;
}

bool HKR5AudioProcessor::laneHasSample (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return false;
    const int side = laneSamp[lane].active.load (std::memory_order_acquire);
    return laneSamp[lane].frames[side].load (std::memory_order_acquire) > 0;
}

juce::String HKR5AudioProcessor::laneSampleName (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return {};
    return laneSamp[lane].name;
}

double HKR5AudioProcessor::laneSampleRate (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return 44100.0;
    return (double) laneSamp[lane].fileRate.load (std::memory_order_relaxed);
}

void HKR5AudioProcessor::copyLaneSample (int lane, std::vector<float>& dest) const
{
    dest.clear();
    if (lane < 0 || lane >= hkr::numLanes)
        return;
    const int side = laneSamp[lane].active.load (std::memory_order_acquire);
    const int n = laneSamp[lane].frames[side].load (std::memory_order_acquire);
    if (n <= 0)
        return;
    dest.assign (laneSamp[lane].buf[side].begin(), laneSamp[lane].buf[side].begin() + n);
}

void HKR5AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    writeBanksToState();
    writeFxOrderToState();
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("uiScale", uiScale);
        copyXmlToBinary (*xml, destData);
    }
}

void HKR5AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            migrateFourKits (*xml);
            uiScale = juce::jlimit (0.4f, 1.5f, (float) xml->getDoubleAttribute ("uiScale", uiScale));
            xml->removeAttribute ("uiScale");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadBanksFromState();
            loadFxOrderFromState();
            loadLaneSamplesFromState();
        }
}

juce::File HKR5AudioProcessor::patchesDir() const
{
    return ensureDir (libraryRoot().getChildFile ("Patches"));
}

juce::File HKR5AudioProcessor::banksDir() const
{
    return ensureDir (libraryRoot().getChildFile ("Banks"));
}

juce::StringArray HKR5AudioProcessor::patchNames() const
{
    return listStemNames (patchesDir(), "*.5hkrp");
}

juce::StringArray HKR5AudioProcessor::bankNames() const
{
    return listStemNames (banksDir(), "*.5hkrb");
}

juce::String HKR5AudioProcessor::patchName() const
{
    return apvts.state.getProperty (kPatchName, kInitPatch).toString();
}

juce::String HKR5AudioProcessor::bankName() const
{
    return apvts.state.getProperty (kBankName, kInitBank).toString();
}

void HKR5AudioProcessor::initPatch()
{
    for (auto* base : getParameters())
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (base);
        if (param == nullptr)
            continue;
        if (param->paramID == ParamID::seqBank || param->paramID == ParamID::seqPattern)
            continue;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    }
    int order[hkr::fxCount];
    publishFxOrder (order, hkr::fillDefaultFxOrder (order));
    apvts.state.setProperty (kPatchName, kInitPatch, nullptr);
    clearLaneSamples();
}

void HKR5AudioProcessor::initBank()
{
    hkr::SlotData slots[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank];
    for (int b = 0; b < hkr::Sequencer::numBanks; ++b)
        for (int p = 0; p < hkr::Sequencer::patternsPerBank; ++p)
            hkr::fillFactory (slots[b][p], b, p);
    applySlots (slots);
    apvts.state.setProperty (kBankName, kInitBank, nullptr);
}

bool HKR5AudioProcessor::loadPatchFile (const juce::File& src, const juce::String& shownName)
{
    auto xml = juce::XmlDocument::parse (src);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;
    migrateFourKits (*xml);
    const auto banks = apvts.state.getProperty (kBanksXml);
    const auto bname = apvts.state.getProperty (kBankName);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    apvts.state.setProperty (kBanksXml, banks, nullptr);
    apvts.state.setProperty (kBankName, bname, nullptr);
    apvts.state.setProperty (kPatchName, shownName, nullptr);
    loadFxOrderFromState();
    loadLaneSamplesFromState();
    return true;
}

bool HKR5AudioProcessor::loadPatchByName (const juce::String& name)
{
    const auto stem = legalStem (name);
    if (stem.isEmpty() || stem == kInitPatch)
    {
        initPatch();
        return true;
    }
    return loadPatchFile (patchesDir().getChildFile (stem + kPatchExt), stem);
}

bool HKR5AudioProcessor::savePatchAs (const juce::String& name)
{
    const auto stem = legalStem (name);
    if (stem.isEmpty() || stem == kInitPatch)
        return false;
    apvts.state.setProperty (kPatchName, stem, nullptr);
    auto tree = apvts.copyState();
    stripPatchExtras (tree);
    auto xml = tree.createXml();
    if (xml == nullptr)
        return false;
    return patchesDir().getChildFile (stem + kPatchExt).replaceWithText (xml->toString());
}

bool HKR5AudioProcessor::savePatch()
{
    const auto name = patchName();
    if (name.isEmpty() || name == kInitPatch)
        return false;
    return savePatchAs (name);
}

bool HKR5AudioProcessor::exportPatch (const juce::File& dest)
{
    auto file = dest.hasFileExtension ("5hkrp") ? dest : dest.withFileExtension ("5hkrp");
    auto tree = apvts.copyState();
    stripPatchExtras (tree);
    auto xml = tree.createXml();
    if (xml == nullptr)
        return false;
    return file.replaceWithText (xml->toString());
}

bool HKR5AudioProcessor::importPatch (const juce::File& src)
{
    const auto stem = legalStem (src.getFileNameWithoutExtension());
    if (stem.isEmpty())
        return false;
    const auto dest = patchesDir().getChildFile (stem + kPatchExt);
    if (src.getFullPathName() != dest.getFullPathName())
    {
        dest.deleteFile();
        if (! src.copyFileTo (dest))
            return false;
    }
    return loadPatchByName (stem);
}

bool HKR5AudioProcessor::loadBankFile (const juce::File& src, const juce::String& shownName)
{
    hkr::SlotData slots[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank];
    if (! hkr::readBankXml (src.loadFileAsString().toStdString(), slots))
        return false;
    applySlots (slots);
    apvts.state.setProperty (kBankName, shownName, nullptr);
    return true;
}

bool HKR5AudioProcessor::loadBankByName (const juce::String& name)
{
    const auto stem = legalStem (name);
    if (stem.isEmpty() || stem == kInitBank)
    {
        initBank();
        return true;
    }
    return loadBankFile (banksDir().getChildFile (stem + kBankExt), stem);
}

bool HKR5AudioProcessor::saveBankAs (const juce::String& name)
{
    const auto stem = legalStem (name);
    if (stem.isEmpty() || stem == kInitBank)
        return false;
    hkr::SlotData slots[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank];
    copySlots (slots);
    apvts.state.setProperty (kBankName, stem, nullptr);
    return banksDir().getChildFile (stem + kBankExt).replaceWithText (hkr::writeBankXml (slots));
}

bool HKR5AudioProcessor::saveBank()
{
    const auto name = bankName();
    if (name.isEmpty() || name == kInitBank)
        return false;
    return saveBankAs (name);
}

bool HKR5AudioProcessor::exportBank (const juce::File& dest)
{
    auto file = dest.hasFileExtension ("5hkrb") ? dest : dest.withFileExtension ("5hkrb");
    hkr::SlotData slots[hkr::Sequencer::numBanks][hkr::Sequencer::patternsPerBank];
    copySlots (slots);
    return file.replaceWithText (hkr::writeBankXml (slots));
}

bool HKR5AudioProcessor::importBank (const juce::File& src)
{
    const auto stem = legalStem (src.getFileNameWithoutExtension());
    if (stem.isEmpty())
        return false;
    const auto dest = banksDir().getChildFile (stem + kBankExt);
    if (src.getFullPathName() != dest.getFullPathName())
    {
        dest.deleteFile();
        if (! src.copyFileTo (dest))
            return false;
    }
    return loadBankByName (stem);
}

juce::AudioProcessorEditor* HKR5AudioProcessor::createEditor()
{
    return new HKR5AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HKR5AudioProcessor();
}
