#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "parameters/Library.h"

#include <cstring>

namespace
{
const juce::Identifier kBanksXml { "BANKS_XML" };
const juce::Identifier kPatchName { "PATCH_NAME" };
const juce::Identifier kBankName { "BANK_NAME" };
const juce::Identifier kFxOrder { "FX_ORDER" };
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

void stripPatchExtras (juce::ValueTree& t)
{
    t.removeProperty (kBanksXml, nullptr);
    t.removeProperty (kBankName, nullptr);
}

} // namespace

HKR5AudioProcessor::HKR5AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
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
    ducker.prepare (sampleRate);
    scFiltL.prepare (sampleRate);
    scFiltR.prepare (sampleRate);
    const int ch = juce::jmax (2, getTotalNumOutputChannels());
    const int n = juce::jmax (samplesPerBlock, 64);
    sendBuf.setSize (ch, n);
    fx.prepare (sampleRate, n, ch);
    postEq.prepare (sampleRate, n, ch);
    analyser.prepare (sampleRate);
}

bool HKR5AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono();
}

float HKR5AudioProcessor::raw (const char* id) const
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    return 0.f;
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

    hkr::SlotData slot;
    snapSlot (currentBank(), currentPattern(), slot);

    float swing[hkr::numLanes];
    float tune[hkr::numLanes];
    float sendAmt[hkr::numLanes];
    uint8_t enabled = 0;
    for (int i = 0; i < hkr::numLanes; ++i)
    {
        swing[i] = raw (ParamID::laneSwing[i]);
        tune[i] = raw (ParamID::laneTune[i]);
        sendAmt[i] = raw (ParamID::laneSend[i]);
        if (raw (ParamID::laneOn[i]) >= 0.5f)
            enabled = (uint8_t) (enabled | (1u << i));
    }
    voices.setTune (tune);

    hkr::Hit hits[64];
    const int n = out.getNumSamples();
    const int nh = sequencer.advance (slot, n, tempoBpm(), run, swing, enabled, hits, 64);
    playheadUi.store (sequencer.playhead(), std::memory_order_relaxed);

    // ponytail: grow only if host exceeds prepare size
    if (sendBuf.getNumSamples() < n || sendBuf.getNumChannels() < out.getNumChannels())
        sendBuf.setSize (juce::jmax (out.getNumChannels(), sendBuf.getNumChannels()),
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
    const bool scKey = scOn || scListen;
    const float thr = raw (ParamID::scThresh);
    const float amt = raw (ParamID::scAmount);
    const float atk = raw (ParamID::scAttack);
    const float rel = raw (ParamID::scRelease);
    const float mix = raw (ParamID::scMix);
    const float hpHz = raw (ParamID::scHp);
    const float lpHz = raw (ParamID::scLp);

    float lanes[hkr::numLanes];
    const int packed = pendingAudition.exchange (-1, std::memory_order_relaxed);
    if (packed >= 0)
        voices.trigger (packed >> 1, (packed & 1) != 0);
    for (int i = 0; i < n; ++i)
    {
        for (int h = 0; h < nh; ++h)
            if (hits[h].offset == i)
                voices.trigger (hits[h].lane, hits[h].accent);
        voices.process (lanes);
        float dry = 0.f, snd = 0.f;
        for (int ln = 0; ln < hkr::numLanes; ++ln)
        {
            dry += lanes[ln] * (1.f - sendAmt[ln]);
            snd += lanes[ln] * sendAmt[ln];
        }
        dry *= vol;
        snd *= vol;
        l[i] = dry;
        sL[i] = snd;
        if (r != nullptr)
            r[i] = dry;
        if (sR != nullptr)
            sR[i] = snd;
    }

    auto sendView = juce::AudioBuffer<float> (sendBuf.getArrayOfWritePointers(),
                                              out.getNumChannels(), n);
    fx.process (sendView, tempoBpm(), apvts, fxOrder.load (std::memory_order_relaxed));
    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.addFrom (ch, 0, sendBuf, ch, 0, n);

    hkr::EqBandParam bands[hkr::eqBandCount];
    hkr::readEqBands (apvts, bands);
    postEq.process (out, hkr::postEqEnabled (apvts), bands);

    if (scOn || scListen)
    {
        for (int i = 0; i < n; ++i)
        {
            float fL = 0.f, fR = 0.f;
            if (scKey)
            {
                fL = scFiltL.process (scL[i], hpHz, lpHz);
                fR = scFiltR.process (scR[i], hpHz, lpHz);
            }
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
            uiScale = juce::jlimit (0.4f, 1.5f, (float) xml->getDoubleAttribute ("uiScale", uiScale));
            xml->removeAttribute ("uiScale");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadBanksFromState();
            loadFxOrderFromState();
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
    const auto banks = apvts.state.getProperty (kBanksXml);
    const auto bname = apvts.state.getProperty (kBankName);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    apvts.state.setProperty (kBanksXml, banks, nullptr);
    apvts.state.setProperty (kBankName, bname, nullptr);
    apvts.state.setProperty (kPatchName, shownName, nullptr);
    loadFxOrderFromState();
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
