#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kEditorW = 980;
constexpr int kEditorH = 560;
constexpr const char* kPageNames[] = { "Main", "Mixer", "Effects", "EQ", "Sidechain" };
constexpr int kPageCount = 5;
constexpr int kPad = 8;
constexpr int kTitleH = 40;
constexpr int kTitleHitW = 150;
constexpr int kLabelH = 12;

const juce::Colour kChassis { 0xff1a2332 };
const juce::Colour kMagenta { 0xffff00ff };
const juce::Colour kCyan    { 0xff00ffff };
const juce::Colour kDark    { 0xff000000 };
const juce::Colour kWhite   { 0xffffffff };
const juce::Colour kMetal   { 0xffc8d6e5 };

juce::Font boldFont (float h)
{
    return juce::Font (juce::FontOptions (h).withStyle ("Bold"));
}

constexpr int kMenuInit = 1;
constexpr int kMenuSave = 2;
constexpr int kMenuSaveAs = 3;
constexpr int kMenuExport = 4;
constexpr int kMenuImport = 5;
constexpr int kMenuFiles = 100;

bool isAudioFile (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("wav;aif;aiff;flac;ogg");
}

const juce::File firstAudioFile (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile (f))
            return juce::File (f);
    return {};
}
} // namespace

void HKR5AudioProcessorEditor::Knob::attach (juce::AudioProcessorValueTreeState& st,
                                            const juce::String& id)
{
    att.reset();
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
}

HKR5AudioProcessorEditor::Knob::Knob (const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, kCyan);
    addAndMakeVisible (label);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 12);
    slider.setColour (juce::Slider::textBoxTextColourId, kWhite);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

HKR5AudioProcessorEditor::Knob::Knob (juce::AudioProcessorValueTreeState& st,
                                      const juce::String& id, const juce::String& name)
    : Knob (name)
{
    attach (st, id);
}

void HKR5AudioProcessorEditor::Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (labelH));
    slider.setBounds (r);
}

HKR5AudioProcessorEditor::LaneRow::LaneRow (HKR5AudioProcessorEditor& ed, int ln)
    : editor (ed), lane (ln)
{
    onBtn.setButtonText (hkr::laneNames[ln]);
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        ed.proc.apvts, ParamID::laneOn[ln], onBtn);
    addAndMakeVisible (onBtn);
    for (int i = 0; i < hkr::numKits; ++i)
        kitBox.addItem (hkr::kitNames[i], i + 1);
    kitAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        ed.proc.apvts, ParamID::laneKit[ln], kitBox);
    addAndMakeVisible (kitBox);
    editBtn.onClick = [this]
    {
        if (editor.editLane == lane)
            editor.leaveSoundEdit();
        else
            editor.openSoundEdit (lane);
    };
    addAndMakeVisible (editBtn);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& lab)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (s);
        lab.setJustificationType (juce::Justification::centred);
        lab.setColour (juce::Label::textColourId, kCyan);
        lab.setFont (boldFont (9.f));
        addAndMakeVisible (lab);
    };
    setupKnob (tune, tuneLabel);
    setupKnob (swing, swingLabel);
    setupKnob (send, sendLabel);
    tuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        ed.proc.apvts, ParamID::laneTune[ln], tune);
    swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        ed.proc.apvts, ParamID::laneSwing[ln], swing);
    sendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        ed.proc.apvts, ParamID::laneSend[ln], send);
    tune.onValueChange = [this] { refreshKnobLabels(); };
    swing.onValueChange = [this] { refreshKnobLabels(); };
    send.onValueChange = [this] { refreshKnobLabels(); };
    refreshKnobLabels();
}

void HKR5AudioProcessorEditor::LaneRow::refreshKnobLabels()
{
    const float st = (float) tune.getValue();
    tuneLabel.setText ("Tune " + juce::String (st, 1), juce::dontSendNotification);
    swingLabel.setText ("Swing " + juce::String (juce::roundToInt ((float) swing.getValue() * 100.f)) + "%",
                        juce::dontSendNotification);
    sendLabel.setText ("FX " + juce::String (juce::roundToInt ((float) send.getValue() * 100.f)) + "%",
                       juce::dontSendNotification);
}

void HKR5AudioProcessorEditor::LaneRow::resized()
{
    auto r = getLocalBounds();
    auto drum = r.removeFromLeft (64).reduced (1);
    onBtn.setBounds (drum.removeFromTop (drum.getHeight() / 2));
    kitBox.setBounds (drum);
    editBtn.setBounds (r.removeFromLeft (44).reduced (2, 6));
    for (juce::Component* c : { (juce::Component*) &tune, (juce::Component*) &tuneLabel,
                                (juce::Component*) &swing, (juce::Component*) &swingLabel,
                                (juce::Component*) &send, (juce::Component*) &sendLabel })
        c->setVisible (! compact);
    if (compact)
    {
        for (auto& c : cells)
            c = {};
        return;
    }
    auto place = [] (juce::Slider& s, juce::Label& lab, juce::Rectangle<int> col)
    {
        lab.setBounds (col.removeFromBottom (12));
        s.setBounds (col.reduced (1));
    };
    place (tune, tuneLabel, r.removeFromLeft (52));
    place (swing, swingLabel, r.removeFromLeft (52));
    place (send, sendLabel, r.removeFromLeft (52));
    const int cellW = juce::jmax (1, r.getWidth() / 16);
    for (int i = 0; i < 16; ++i)
        cells[i] = { r.getX() + i * cellW, r.getY() + 4, cellW - 2, r.getHeight() - 8 };
}

void HKR5AudioProcessorEditor::LaneRow::paint (juce::Graphics& g)
{
    if (hot)
    {
        g.setColour (kCyan.withAlpha (0.22f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.f), 3.f);
    }
    if (compact)
        return;
    for (int i = 0; i < 16; ++i)
    {
        auto c = cells[i].toFloat();
        if (values[i] == 2)
            g.setColour (kMagenta);
        else if (values[i] == 1)
            g.setColour (kCyan.withAlpha (0.85f));
        else
            g.setColour (kDark.brighter (0.08f));
        g.fillRoundedRectangle (c, 3.f);
        if (loopCell == i)
        {
            g.setColour (kMagenta);
            g.drawRoundedRectangle (c.reduced (0.5f), 3.f, 2.f);
        }
        if (play == i)
        {
            g.setColour (kWhite);
            g.drawRoundedRectangle (c, 3.f, 1.5f);
        }
        else if (loopCell != i && (i % 4) == 0)
        {
            g.setColour (kWhite.withAlpha (0.25f));
            g.drawRoundedRectangle (c, 3.f, 1.f);
        }
    }
}

void HKR5AudioProcessorEditor::LaneRow::mouseDown (const juce::MouseEvent& e)
{
    if (compact)
        return;
    for (int i = 0; i < 16; ++i)
    {
        if (! cells[i].contains (e.getPosition()))
            continue;
        const int idx = editor.stepPage * 16 + i;
        if (e.mods.isPopupMenu())
            editor.proc.toggleLoopEnd (lane, idx);
        else
        {
            values[i] = editor.proc.cycleStep (lane, idx);
            if (values[i] > 0)
                editor.proc.audition (lane, values[i] == 2);
        }
        editor.refreshGrid();
        return;
    }
}

bool HKR5AudioProcessorEditor::LaneRow::isInterestedInFileDrag (const juce::StringArray& files)
{
    return firstAudioFile (files).existsAsFile();
}

void HKR5AudioProcessorEditor::LaneRow::fileDragEnter (const juce::StringArray&, int, int)
{
    hot = true;
    repaint();
}

void HKR5AudioProcessorEditor::LaneRow::fileDragExit (const juce::StringArray&)
{
    hot = false;
    repaint();
}

void HKR5AudioProcessorEditor::LaneRow::filesDropped (const juce::StringArray& files, int, int)
{
    hot = false;
    repaint();
    const auto file = firstAudioFile (files);
    if (file.existsAsFile())
        editor.loadSampleFile (lane, file);
}

void HKR5AudioProcessorEditor::LaneRow::refresh (const hkr::SlotData& slot, int playhead, int page)
{
    play = -1;
    loopCell = -1;
    const int base = page * 16;
    for (int i = 0; i < 16; ++i)
        values[i] = hkr::getStep (slot, lane, base + i);
    const int lp = juce::jlimit (1, (int) slot.len, (int) slot.loop[lane]);
    const int endIdx = lp - 1;
    if (endIdx >= base && endIdx < base + 16)
        loopCell = endIdx - base;
    if (playhead >= 0)
    {
        const int ls = hkr::laneStep (playhead, lp);
        if (ls >= base && ls < base + 16)
            play = ls - base;
    }
    refreshKnobLabels();
    repaint();
}

void HKR5AudioProcessorEditor::SlotBar::setText (const juce::String& t)
{
    if (text == t)
        return;
    text = t;
    repaint();
}

void HKR5AudioProcessorEditor::SlotBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kDark);
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (kCyan.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);
    auto left = r.removeFromLeft (18.f);
    auto right = r.removeFromRight (18.f);
    auto chev = [] (juce::Rectangle<float> box, bool back)
    {
        juce::Path p;
        const float cx = box.getCentreX();
        const float cy = box.getCentreY();
        if (back)
            p.addTriangle (cx + 3.f, cy - 5.f, cx - 4.f, cy, cx + 3.f, cy + 5.f);
        else
            p.addTriangle (cx - 3.f, cy - 5.f, cx + 4.f, cy, cx - 3.f, cy + 5.f);
        return p;
    };
    g.setColour (kCyan);
    g.fillPath (chev (left, true));
    g.fillPath (chev (right, false));
    g.setFont (boldFont (12.f));
    g.setColour (kWhite);
    g.drawText (text, r.toNearestInt(), juce::Justification::centred, false);
}

void HKR5AudioProcessorEditor::SlotBar::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;
    const int w = getWidth();
    if (e.x < 20)
    {
        if (onStep)
            onStep (-1);
    }
    else if (e.x > w - 20)
    {
        if (onStep)
            onStep (1);
    }
    else if (onOpen)
        onOpen();
}

HKR5AudioProcessorEditor::VolumeStrip::VolumeStrip (HKR5AudioProcessorEditor& ed)
    : editor (ed)
{
    slider.setLookAndFeel (&lnf);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setOpaque (false);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (editor.proc.apvts.getParameter (ParamID::volume));
    if (p != nullptr)
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        editor.proc.apvts, ParamID::volume, slider);
    addAndMakeVisible (slider);
}

HKR5AudioProcessorEditor::VolumeStrip::~VolumeStrip()
{
    slider.setLookAndFeel (nullptr);
}

juce::Rectangle<float> HKR5AudioProcessorEditor::VolumeStrip::faderArea() const
{
    auto r = getLocalBounds().toFloat();
    return r.removeFromLeft (r.getWidth() * 5.f / 9.f);
}

juce::Rectangle<float> HKR5AudioProcessorEditor::VolumeStrip::meterArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (r.getWidth() * 5.f / 9.f + 4.f);
    return r;
}

void HKR5AudioProcessorEditor::VolumeStrip::resized()
{
    slider.setBounds (faderArea().toNearestInt());
}

void HKR5AudioProcessorEditor::VolumeStrip::LnF::drawLinearSlider (juce::Graphics& g, int, int y, int, int h,
                                                                  float pos, float, float,
                                                                  juce::Slider::SliderStyle, juce::Slider&)
{
    const float by = (float) (y + h);
    juce::Path p;
    p.addTriangle (pos - 5.f, by, pos + 5.f, by, pos, by - 7.f);
    g.setColour (kDark);
    g.fillPath (p);
    g.setColour (kMagenta);
    g.strokePath (p, juce::PathStrokeType (1.f));
}

void HKR5AudioProcessorEditor::VolumeStrip::paint (juce::Graphics& g)
{
    auto fader = faderArea();
    auto meter = meterArea();
    g.setColour (kDark);
    g.fillRoundedRectangle (fader, 3.f);
    g.fillRoundedRectangle (meter, 3.f);

    const float pos = (float) slider.valueToProportionOfLength (slider.getValue());
    auto fill = fader.withWidth (juce::jmax (2.f, fader.getWidth() * pos));
    g.setColour (clipped ? kMagenta : kCyan.withAlpha (0.7f));
    g.fillRoundedRectangle (fill, 3.f);

    const float peak = hkr::meterToPos (editor.proc.outputMeter().lastPeak());
    auto pfill = meter.withWidth (juce::jmax (2.f, meter.getWidth() * peak));
    g.setColour (clipped ? kMagenta : kCyan);
    g.fillRoundedRectangle (pfill, 3.f);
}

void HKR5AudioProcessorEditor::VolumeStrip::refresh()
{
    if (editor.proc.outputMeter().takeClip())
        clipUntil = juce::Time::getMillisecondCounter() + 800;
    clipped = juce::Time::getMillisecondCounter() < clipUntil;
    repaint();
}

void HKR5AudioProcessorEditor::PanelLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                                           float pos, float startAngle, float endAngle,
                                                           juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
    const float d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto rc = bounds.withSizeKeepingCentre (d, d);
    g.setColour (kDark);
    g.fillEllipse (rc);
    auto cap = rc.reduced (d * 0.16f);
    g.setColour (kCyan);
    g.fillEllipse (cap);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto c = cap.getCentre();
    const auto needle = c.getPointOnCircumference (cap.getWidth() * 0.42f, angle);
    g.setColour (kDark);
    g.drawLine (c.x, c.y, needle.x, needle.y, 2.2f);
}

void HKR5AudioProcessorEditor::PanelLnF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (2.f);
    g.setColour (b.getToggleState() ? kMagenta.withAlpha (0.85f) : kDark);
    g.fillRoundedRectangle (bounds, 3.f);
    g.setFont (boldFont (11.f));
    g.setColour (b.getToggleState() ? kWhite : kCyan);
    g.drawText (b.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, false);
}

void HKR5AudioProcessorEditor::PanelLnF::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                                       int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.f, 0.f, (float) width, (float) height);
    g.setColour (kDark);
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (kCyan.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
}

juce::Font HKR5AudioProcessorEditor::PanelLnF::getLabelFont (juce::Label&)
{
    return boldFont (11.f);
}

juce::Font HKR5AudioProcessorEditor::PanelLnF::getComboBoxFont (juce::ComboBox&)
{
    return boldFont (11.f);
}

HKR5AudioProcessorEditor::AboutBox::AboutBox()
{
    closeBtn.setClickingTogglesState (false);
    closeBtn.onClick = [this] { setVisible (false); };
    addAndMakeVisible (closeBtn);
}

void HKR5AudioProcessorEditor::AboutBox::paint (juce::Graphics& g)
{
    g.fillAll (kDark.withAlpha (0.65f));
    auto card = getLocalBounds().withSizeKeepingCentre (320, 160).toFloat();
    g.setColour (kChassis);
    g.fillRoundedRectangle (card, 8.f);
    g.setColour (kMagenta);
    g.drawRoundedRectangle (card, 8.f, 2.f);
    g.setColour (kWhite);
    g.setFont (boldFont (18.f));
    g.drawText ("5HKR  v" JucePlugin_VersionString, card.removeFromTop (48).toNearestInt(),
                juce::Justification::centred);
    g.setFont (boldFont (13.f));
    g.setColour (kCyan);
    g.drawText ("Stupid Systems LLC", card.removeFromTop (28).toNearestInt(),
                juce::Justification::centred);
    g.setColour (kWhite.withAlpha (0.8f));
    g.drawText ("Hold C1 for bank 1 pattern 1.", card.removeFromTop (24).toNearestInt(),
                juce::Justification::centred);
}

void HKR5AudioProcessorEditor::AboutBox::resized()
{
    auto card = getLocalBounds().withSizeKeepingCentre (320, 160);
    closeBtn.setBounds (card.removeFromBottom (36).reduced (80, 6));
}

void HKR5AudioProcessorEditor::ScSpec::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff12151a));
    g.fillRoundedRectangle (bounds, 4.f);

    auto r = bounds.reduced (28.f, 18.f);
    if (r.getWidth() < 2.f || r.getHeight() < 2.f)
        return;

    auto hzToX = [] (float hz)
    {
        hz = juce::jlimit (20.f, 20000.f, hz);
        return std::log10 (hz / 20.f) / 3.f;
    };
    auto specToY = [] (float db)
    {
        return 1.f - juce::jlimit (0.f, 1.f, (db + 72.f) / 72.f);
    };

    g.setColour (juce::Colour (0xff2a3444));
    const float gridHz[] = { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f };
    for (float hz : gridHz)
        g.drawVerticalLine ((int) (r.getX() + hzToX (hz) * r.getWidth()), r.getY(), r.getBottom());

    const int n = juce::jmax (2, (int) r.getWidth());
    if ((int) specDb.size() != n)
        specDb.resize ((size_t) n);
    proc.scAnalyser().copyLogDb (specDb.data(), n);
    juce::Path spec;
    spec.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i < n; ++i)
    {
        const float x01 = (float) i / (float) (n - 1);
        spec.lineTo (r.getX() + x01 * r.getWidth(),
                     r.getY() + specToY (specDb[(size_t) i]) * r.getHeight());
    }
    spec.lineTo (r.getRight(), r.getBottom());
    spec.closeSubPath();
    g.setColour (juce::Colour (0x55d94214));
    g.fillPath (spec);

    auto raw = [this] (const char* id) -> float
    {
        if (auto* v = proc.apvts.getRawParameterValue (id))
            return v->load();
        return 0.f;
    };
    g.setColour (kCyan.withAlpha (0.45f));
    g.drawVerticalLine ((int) (r.getX() + hzToX (raw (ParamID::scHp)) * r.getWidth()),
                        r.getY(), r.getBottom());
    g.drawVerticalLine ((int) (r.getX() + hzToX (raw (ParamID::scLp)) * r.getWidth()),
                        r.getY(), r.getBottom());

    g.setColour (kMetal.withAlpha (0.7f));
    g.setFont (boldFont (9.f));
    g.drawText ("20", r.withHeight (14.f).translated (0.f, r.getHeight()), juce::Justification::centredLeft);
    g.drawText ("20k", r.withHeight (14.f).translated (0.f, r.getHeight()), juce::Justification::centredRight);
}

HKR5AudioProcessorEditor::HKR5AudioProcessorEditor (HKR5AudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p), mixerPage (p), effectsPage (p), eqPage (p)
{
    setLookAndFeel (&panelLnF);
    setOpaque (true);
    panelLnF.setColour (juce::PopupMenu::backgroundColourId, kDark);
    panelLnF.setColour (juce::PopupMenu::textColourId, kWhite);
    panelLnF.setColour (juce::PopupMenu::highlightedBackgroundColourId, kMagenta);
    panelLnF.setColour (juce::ComboBox::textColourId, kWhite);
    panelLnF.setColour (juce::ComboBox::arrowColourId, kCyan);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    pageBar.setText (kPageNames[0]);
    pageBar.onStep = [this] (int d) { setEditorPage ((editorPage + d + kPageCount) % kPageCount); };
    pageBar.onOpen = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < kPageCount; ++i)
            m.addItem (i + 1, kPageNames[i], true, i == editorPage);
        juce::Component::SafePointer<HKR5AudioProcessorEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (pageBar),
                         [safe] (int r)
                         {
                             if (safe != nullptr && r > 0)
                                 safe->setEditorPage (r - 1);
                         });
    };
    content.addAndMakeVisible (pageBar);

    patchBar.onStep = [this] (int d) { cycleLib (false, d); };
    patchBar.onOpen = [this] { openLibMenu (false); };
    bankBar.onStep = [this] (int d) { cycleLib (true, d); };
    bankBar.onOpen = [this] { openLibMenu (true); };
    content.addAndMakeVisible (patchBar);
    content.addAndMakeVisible (bankBar);
    content.addAndMakeVisible (volumeStrip);

    for (int i = 0; i < hkr::Sequencer::numBanks; ++i)
        bankBox.addItem ("Bank " + juce::String (i + 1), i + 1);
    for (int i = 0; i < hkr::Sequencer::patternsPerBank; ++i)
        patBox.addItem ("Pat " + juce::String (i + 1), i + 1);
    bankAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, ParamID::seqBank, bankBox);
    patAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, ParamID::seqPattern, patBox);
    content.addAndMakeVisible (bankBox);
    content.addAndMakeVisible (patBox);

    lenBox.addItem ("1 bar", 1);
    lenBox.addItem ("2 bars", 2);
    lenBox.addItem ("4 bars", 3);
    lenBox.onChange = [this]
    {
        const int choice = lenBox.getSelectedId();
        proc.setPatLen (choice == 3 ? 64 : choice == 2 ? 32 : 16);
        stepPage = 0;
        refreshGrid();
    };
    content.addAndMakeVisible (lenBox);

    pageLeft.onClick = [this]
    {
        stepPage = juce::jmax (0, stepPage - 1);
        refreshGrid();
    };
    pageRight.onClick = [this]
    {
        const auto slot = proc.currentSlot();
        const int pages = juce::jmax (1, (int) slot.len / 16);
        stepPage = juce::jmin (pages - 1, stepPage + 1);
        refreshGrid();
    };
    content.addAndMakeVisible (pageLeft);
    content.addAndMakeVisible (pageRight);
    pageLabel.setJustificationType (juce::Justification::centred);
    pageLabel.setColour (juce::Label::textColourId, kCyan);
    content.addAndMakeVisible (pageLabel);
    randomBtn.onClick = [this]
    {
        proc.randomizePattern();
        refreshGrid();
    };
    clearBtn.onClick = [this]
    {
        proc.clearPattern();
        refreshGrid();
    };
    content.addAndMakeVisible (randomBtn);
    content.addAndMakeVisible (clearBtn);

    for (int i = 0; i < hkr::numLanes; ++i)
    {
        auto* row = rows.add (new LaneRow (*this, i));
        content.addAndMakeVisible (row);
    }

    content.addChildComponent (wave);
    wave.editor = this;
    content.addChildComponent (editStrip);
    content.addChildComponent (editLoad);
    content.addChildComponent (editReverse);
    editLoad.onClick = [this] { chooseLaneSample(); };
    static constexpr const char* kEditNames[] = {
        "Tune", "Decay", "Click", "Drive", "HPF", "Tone", "Level"
    };
    for (auto* name : kEditNames)
    {
        auto* k = editKnobs.add (new Knob (name));
        k->labelH = 16;
        k->label.setFont (boldFont (12.f));
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        content.addChildComponent (k);
    }
    listenLaneParams (true);

    scOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, ParamID::scOn, scOn);
    scListenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, ParamID::scListen, scListen);
    content.addAndMakeVisible (scOn);
    content.addAndMakeVisible (scListen);
    struct Spec { const char* id; const char* name; };
    static constexpr Spec kSc[] = {
        { ParamID::scHp, "HP" }, { ParamID::scLp, "LP" },
        { ParamID::scThresh, "Thresh" }, { ParamID::scAmount, "Amount" },
        { ParamID::scAttack, "Attack" }, { ParamID::scRelease, "Release" },
        { ParamID::scMix, "Mix" }
    };
    for (auto& s : kSc)
    {
        auto* k = scKnobs.add (new Knob (proc.apvts, s.id, s.name));
        content.addAndMakeVisible (k);
    }
    content.addAndMakeVisible (scSpec);

    content.addChildComponent (mixerPage);
    content.addChildComponent (effectsPage);
    content.addChildComponent (eqPage);
    mixerPage.setVisible (false);
    effectsPage.setVisible (false);
    eqPage.setVisible (false);

    content.addChildComponent (aboutBox);
    aboutBox.setVisible (false);

    baseW = kEditorW;
    baseH = kEditorH;
    content.setSize (baseW, baseH);
    addAndMakeVisible (content);

    const float s = juce::jlimit (0.4f, 1.5f, proc.uiScale);
    setResizable (true, true);
    setResizeLimits (baseW * 2 / 5, baseH * 2 / 5, baseW * 3 / 2, baseH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (juce::roundToInt ((float) baseW * s), juce::roundToInt ((float) baseH * s));
    startTimerHz (20);
    refreshBars();
    refreshGrid();
}

HKR5AudioProcessorEditor::~HKR5AudioProcessorEditor()
{
    listenLaneParams (false);
    stopTimer();
    setLookAndFeel (nullptr);
}

void HKR5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kChassis);
}

void HKR5AudioProcessorEditor::resized()
{
    if (baseW == 0)
        return;
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (s));
    proc.uiScale = s;
}

void HKR5AudioProcessorEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (kChassis);
    auto r = content.getLocalBounds().reduced (kPad);
    auto title = r.removeFromTop (kTitleH);
    if (logo.isValid())
    {
        auto logoR = title.removeFromLeft (kTitleH).reduced (2);
        g.drawImage (logo, logoR.toFloat(), juce::RectanglePlacement::centred);
        title.removeFromLeft (6);
    }
    auto nameCol = title.removeFromLeft (90);
    g.setColour (kMagenta);
    g.setFont (boldFont (16.f));
    g.drawText ("5HKR", nameCol.removeFromTop (22), juce::Justification::centredLeft, false);
    g.setFont (boldFont (10.f));
    g.setColour (kCyan);
    g.drawText ("v" JucePlugin_VersionString, nameCol, juce::Justification::centredLeft, false);
}

void HKR5AudioProcessorEditor::layoutContent()
{
    auto r = content.getLocalBounds().reduced (kPad);
    auto title = r.removeFromTop (kTitleH);
    title.removeFromLeft (kTitleHitW);
    volumeStrip.setBounds (title.removeFromRight (240).reduced (2, 4));
    pageBar.setBounds (title.removeFromLeft (108).reduced (4, 4));
    const int barW = title.getWidth() / 2;
    patchBar.setBounds (title.removeFromLeft (barW).reduced (6, 2));
    bankBar.setBounds (title.reduced (6, 2));

    const bool editing = editorPage == 0 && editLane >= 0;
    // Tools strip stays reserved in edit mode so the lane column does not jump.
    auto tools = r.removeFromTop (28);
    bankBox.setBounds (tools.removeFromLeft (90).reduced (2, 2));
    patBox.setBounds (tools.removeFromLeft (90).reduced (2, 2));
    clearBtn.setBounds (tools.removeFromRight (72).reduced (2, 2));
    randomBtn.setBounds (tools.removeFromRight (72).reduced (2, 2));
    auto mid = tools.withSizeKeepingCentre (90 + 28 + 72 + 28, tools.getHeight());
    lenBox.setBounds (mid.removeFromLeft (90).reduced (2, 2));
    pageLeft.setBounds (mid.removeFromLeft (28).reduced (2, 2));
    pageLabel.setBounds (mid.removeFromLeft (72));
    pageRight.setBounds (mid.removeFromLeft (28).reduced (2, 2));

    constexpr int kLaneColW = 108; // LaneRow drum 64 + Edit 44
    const int rowH = juce::jmax (1, r.getHeight() / hkr::numLanes);
    for (int i = 0; i < rows.size(); ++i)
    {
        rows[i]->compact = editing;
        rows[i]->setBounds (r.getX(), r.getY() + i * rowH,
                            editing ? kLaneColW : r.getWidth(), rowH);
        rows[i]->resized();
        rows[i]->repaint();
    }

    if (editing)
    {
        auto body = r.withTrimmedLeft (kLaneColW + 6);
        editStrip.setBounds (body.removeFromRight (120).reduced (4, 0));
        body.removeFromRight (6);
        wave.setBounds (body.removeFromTop (250));
        body.removeFromTop (8);
        auto knobRow = body.removeFromTop (juce::jmin (body.getHeight(), 200));
        if (editIsSample)
        {
            editLoad.setBounds (knobRow.removeFromLeft (76).reduced (4, 36));
            editReverse.setBounds (knobRow.removeFromLeft (88).reduced (4, 36));
        }
        int shown = 0;
        for (int i = 0; i < editKnobs.size(); ++i)
            if (! editIsSample || i == 0 || i == 1 || i == 2 || i == 6)
                ++shown;
        const int cellW = juce::jmax (1, knobRow.getWidth() / juce::jmax (1, shown));
        for (int i = 0; i < editKnobs.size(); ++i)
        {
            const bool show = ! editIsSample || i == 0 || i == 1 || i == 2 || i == 6;
            if (show)
                editKnobs[i]->setBounds (knobRow.removeFromLeft (cellW));
        }
    }

    auto sc = r;
    auto scToggles = sc.removeFromTop (28);
    scOn.setBounds (scToggles.removeFromLeft (110));
    scListen.setBounds (scToggles.removeFromLeft (90));
    const int knH = juce::jmin (140, juce::jmax (1, sc.getHeight() / 2));
    const int kw = juce::jmax (1, sc.getWidth() / juce::jmax (1, scKnobs.size()));
    for (int i = 0; i < scKnobs.size(); ++i)
        scKnobs[i]->setBounds (sc.getX() + i * kw, sc.getY(), kw, knH);
    sc.removeFromTop (knH);
    scSpec.setBounds (sc);

    mixerPage.setBounds (r);
    effectsPage.setBounds (r);
    eqPage.setBounds (r);

    aboutBox.setBounds (content.getLocalBounds());
    applyPageVisibility();
}

void HKR5AudioProcessorEditor::timerCallback()
{
    if (editLane >= 0)
    {
        if (waveDirty)
            rebuildWave();
        const float env = proc.voiceEnv (editLane);
        const float age = proc.voiceAgeSec (editLane);
        const float dur = juce::jmax (0.001f, wave.durSec);
        const float x01 = env > 0.002f ? age / dur : -1.f;
        if (x01 != wave.play01)
        {
            wave.play01 = x01;
            wave.repaint();
        }
        editStrip.tickMeter();
    }
    volumeStrip.refresh();
    refreshBars();
    refreshGrid();
    if (editorPage == 2)
        effectsPage.refresh();
    if (editorPage == 4)
        scSpec.repaint();
    // MIDI slot select does not write APVTS; keep combos in sync.
    bankBox.setSelectedItemIndex (proc.currentBank(), juce::dontSendNotification);
    patBox.setSelectedItemIndex (proc.currentPattern(), juce::dontSendNotification);
}

void HKR5AudioProcessorEditor::refreshGrid()
{
    const auto slot = proc.currentSlot();
    const int pages = juce::jmax (1, (int) slot.len / 16);
    stepPage = juce::jlimit (0, pages - 1, stepPage);
    pageLabel.setText ("Steps " + juce::String (stepPage * 16 + 1) + "-" + juce::String (stepPage * 16 + 16),
                       juce::dontSendNotification);
    const int wantId = slot.len >= 64 ? 3 : slot.len >= 32 ? 2 : 1;
    if (lenBox.getSelectedId() != wantId)
        lenBox.setSelectedId (wantId, juce::dontSendNotification);
    const int ph = proc.playhead();
    for (auto* row : rows)
        row->refresh (slot, ph, stepPage);
}

void HKR5AudioProcessorEditor::refreshBars()
{
    patchBar.setText (proc.patchName());
    bankBar.setText (proc.bankName());
}

juce::Rectangle<int> HKR5AudioProcessorEditor::titleHitBounds() const
{
    return { kPad, kPad, kTitleHitW, kTitleH };
}

void HKR5AudioProcessorEditor::setEditorPage (int page)
{
    editorPage = juce::jlimit (0, kPageCount - 1, page);
    if (editorPage != 0)
    {
        editLane = -1;
        startTimerHz (20);
    }
    pageBar.setText (kPageNames[editorPage]);
    layoutContent();
}

void HKR5AudioProcessorEditor::applyPageVisibility()
{
    const bool main = editorPage == 0;
    const bool editing = main && editLane >= 0;
    const bool mix = editorPage == 1;
    const bool fx = editorPage == 2;
    const bool eq = editorPage == 3;
    const bool sc = editorPage == 4;
    bankBox.setVisible (true);
    patBox.setVisible (true);
    lenBox.setVisible (true);
    pageLeft.setVisible (true);
    pageRight.setVisible (true);
    pageLabel.setVisible (true);
    randomBtn.setVisible (true);
    clearBtn.setVisible (true);
    for (auto* row : rows)
    {
        row->setVisible (main);
        if (editing && row->lane == editLane)
        {
            row->editBtn.setColour (juce::TextButton::buttonColourId, kMagenta);
            row->editBtn.setColour (juce::TextButton::textColourOffId, kWhite);
        }
        else
        {
            row->editBtn.removeColour (juce::TextButton::buttonColourId);
            row->editBtn.removeColour (juce::TextButton::textColourOffId);
        }
    }
    wave.setVisible (editing);
    editStrip.setVisible (editing);
    const bool sampleEdit = editing && editIsSample;
    editLoad.setVisible (sampleEdit);
    editReverse.setVisible (sampleEdit);
    for (int i = 0; i < editKnobs.size(); ++i)
    {
        const bool show = editing && (! sampleEdit || i == 0 || i == 1 || i == 2 || i == 6);
        editKnobs[i]->setVisible (show);
    }
    scOn.setVisible (sc);
    scListen.setVisible (sc);
    scSpec.setVisible (sc);
    for (auto* k : scKnobs)
        k->setVisible (sc);
    mixerPage.setVisible (mix);
    effectsPage.setVisible (fx);
    eqPage.setVisible (eq);
    if (fx)
        effectsPage.refresh();
}

void HKR5AudioProcessorEditor::listenLaneParams (bool on)
{
    auto one = [this, on] (const char* const* group)
    {
        for (int i = 0; i < hkr::numLanes; ++i)
        {
            if (on)
                proc.apvts.addParameterListener (group[i], this);
            else
                proc.apvts.removeParameterListener (group[i], this);
        }
    };
    one (ParamID::laneKit);
    one (ParamID::laneTune);
    one (ParamID::laneDecay);
    one (ParamID::laneClick);
    one (ParamID::laneHp);
    one (ParamID::laneLp);
    one (ParamID::laneGain);
    one (ParamID::laneDrive);
    one (ParamID::laneSampStart);
    one (ParamID::laneSampEnd);
    one (ParamID::laneSampRev);
}

void HKR5AudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    waveDirty = true;
    if (bindingEdit || editLane < 0)
        return;
    const bool sample = laneIsSample (editLane);
    if (sample != editIsSample)
    {
        bindEditLane();
        layoutContent();
    }
}

void HKR5AudioProcessorEditor::openSoundEdit (int lane)
{
    editLane = juce::jlimit (0, hkr::numLanes - 1, lane);
    bindEditLane();
    proc.audition (editLane, false);
    startTimerHz (50);
    layoutContent();
}

void HKR5AudioProcessorEditor::leaveSoundEdit()
{
    editLane = -1;
    startTimerHz (20);
    layoutContent();
}

void HKR5AudioProcessorEditor::bindEditLane()
{
    if (editLane < 0)
        return;
    const int ln = editLane;
    editIsSample = laneIsSample (ln);
    const bool sample = editIsSample;
    static constexpr const char* kSynthNames[] = {
        "Tune", "Decay", "Click", "Drive", "HPF", "Tone", "Level"
    };
    static constexpr const char* kSampleNames[] = {
        "Tune", "Start", "End", "", "", "", "Level"
    };
    const char* synthIds[] = {
        ParamID::laneTune[ln], ParamID::laneDecay[ln], ParamID::laneClick[ln],
        ParamID::laneDrive[ln], ParamID::laneHp[ln], ParamID::laneLp[ln],
        ParamID::laneGain[ln]
    };
    const char* sampleIds[] = {
        ParamID::laneTune[ln], ParamID::laneSampStart[ln], ParamID::laneSampEnd[ln],
        nullptr, nullptr, nullptr, ParamID::laneGain[ln]
    };
    bindingEdit = true;
    for (int i = 0; i < editKnobs.size() && i < 7; ++i)
    {
        editKnobs[i]->label.setText (sample ? kSampleNames[i] : kSynthNames[i],
                                     juce::dontSendNotification);
        const char* id = sample ? sampleIds[i] : synthIds[i];
        if (id != nullptr)
            editKnobs[i]->attach (proc.apvts, id);
        else
            editKnobs[i]->att.reset();
    }
    editRevAtt.reset();
    if (sample)
        editRevAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, ParamID::laneSampRev[ln], editReverse);
    bindingEdit = false;
    editStrip.bind (ln);
    waveDirty = true;
}

void HKR5AudioProcessorEditor::rebuildWave()
{
    waveDirty = false;
    if (editLane < 0)
        return;
    if (laneIsSample (editLane))
    {
        rebuildSampleWave();
        return;
    }
    // ~2 px per bucket at the 250px-tall wave width; wider buckets read as a solid shape.
    const int n = 360;
    std::vector<float> full (44100);
    proc.renderLaneShot (editLane, full.data(), (int) full.size(), 44100.0);

    int last = (int) full.size() - 1;
    while (last > 64 && std::abs (full[(size_t) last]) < 0.002f)
        --last;
    last = juce::jmax (64, last);
    wave.durSec = (float) last / 44100.f;

    float peak = 0.f;
    for (int i = 0; i <= last; ++i)
        peak = juce::jmax (peak, std::abs (full[(size_t) i]));
    wave.peakDb = juce::Decibels::gainToDecibels (peak, -100.f);
    const float norm = peak > 1.0e-6f ? 1.f / peak : 0.f;

    // Min/max per bucket so noise reads as a filled burst, not one random sample per pixel.
    wave.lo.assign ((size_t) n, 0.f);
    wave.hi.assign ((size_t) n, 0.f);
    for (int b = 0; b < n; ++b)
    {
        const int s0 = (int) ((double) b / n * (last + 1));
        const int s1 = juce::jmax (s0 + 1, (int) ((double) (b + 1) / n * (last + 1)));
        float mn = 0.f, mx = 0.f;
        for (int i = s0; i < s1 && i <= last; ++i)
        {
            mn = juce::jmin (mn, full[(size_t) i]);
            mx = juce::jmax (mx, full[(size_t) i]);
        }
        wave.lo[(size_t) b] = mn * norm;
        wave.hi[(size_t) b] = mx * norm;
    }

    const int kit = juce::jlimit (0, hkr::numKits - 1,
                                  (int) proc.apvts.getRawParameterValue (ParamID::laneKit[editLane])->load());
    wave.title = juce::String (hkr::laneNames[editLane]) + "  " + hkr::kitNames[kit];
    wave.info = juce::String (juce::roundToInt (wave.durSec * 1000.f)) + " ms";
    wave.repaint();
}

void HKR5AudioProcessorEditor::rebuildSampleWave()
{
    const int nBuckets = 360;
    std::vector<float> pcm;
    proc.copyLaneSample (editLane, pcm);
    float a = proc.apvts.getRawParameterValue (ParamID::laneSampStart[editLane])->load();
    float b = proc.apvts.getRawParameterValue (ParamID::laneSampEnd[editLane])->load();
    if (b < a)
        std::swap (a, b);
    const int frames = (int) pcm.size();
    int i0 = std::clamp ((int) (a * (float) frames), 0, std::max (0, frames - 1));
    int i1 = std::clamp ((int) (b * (float) frames), 0, frames);
    if (frames == 0 || i1 <= i0)
    {
        wave.lo.assign ((size_t) nBuckets, 0.f);
        wave.hi.assign ((size_t) nBuckets, 0.f);
        wave.durSec = 0.001f;
        wave.peakDb = -100.f;
        wave.title = juce::String (hkr::laneNames[editLane]) + "  Sample";
        wave.info = "Drop or Load";
        wave.repaint();
        return;
    }
    const float semi = proc.apvts.getRawParameterValue (ParamID::laneTune[editLane])->load();
    const float ratio = std::pow (2.f, semi / 12.f);
    const double fileRate = std::max (1.0, proc.laneSampleRate (editLane));
    wave.durSec = (float) ((double) (i1 - i0) / (fileRate * (double) ratio));

    float peak = 0.f;
    for (int i = i0; i < i1; ++i)
        peak = juce::jmax (peak, std::abs (pcm[(size_t) i]));
    wave.peakDb = juce::Decibels::gainToDecibels (peak, -100.f);
    const float norm = peak > 1.0e-6f ? 1.f / peak : 0.f;
    wave.lo.assign ((size_t) nBuckets, 0.f);
    wave.hi.assign ((size_t) nBuckets, 0.f);
    const int span = i1 - i0;
    for (int bucket = 0; bucket < nBuckets; ++bucket)
    {
        const int s0 = i0 + (int) ((double) bucket / nBuckets * span);
        const int s1 = juce::jmax (s0 + 1, i0 + (int) ((double) (bucket + 1) / nBuckets * span));
        float mn = 0.f, mx = 0.f;
        for (int i = s0; i < s1 && i < i1; ++i)
        {
            mn = juce::jmin (mn, pcm[(size_t) i]);
            mx = juce::jmax (mx, pcm[(size_t) i]);
        }
        wave.lo[(size_t) bucket] = mn * norm;
        wave.hi[(size_t) bucket] = mx * norm;
    }
    const auto name = proc.laneSampleName (editLane);
    wave.title = juce::String (hkr::laneNames[editLane]) + "  Sample";
    wave.info = name.isEmpty() ? juce::String ("Sample") : name;
    wave.repaint();
}

bool HKR5AudioProcessorEditor::laneIsSample (int lane) const
{
    if (lane < 0 || lane >= hkr::numLanes)
        return false;
    return (int) proc.apvts.getRawParameterValue (ParamID::laneKit[lane])->load() == hkr::kitSample;
}

void HKR5AudioProcessorEditor::loadSampleFile (int lane, const juce::File& file)
{
    if (! proc.loadLaneSample (lane, file))
        return;
    if (auto* p = proc.apvts.getParameter (ParamID::laneKit[lane]))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) hkr::kitSample));
        p->endChangeGesture();
    }
    waveDirty = true;
    if (editLane == lane)
    {
        bindEditLane();
        layoutContent();
    }
}

void HKR5AudioProcessorEditor::chooseLaneSample()
{
    if (editLane < 0)
        return;
    const int lane = editLane;
    chooser = std::make_unique<juce::FileChooser> (
        "Load Sample",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    juce::Component::SafePointer<HKR5AudioProcessorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [safe, lane] (const juce::FileChooser& c)
                          {
                              if (safe == nullptr)
                                  return;
                              const auto file = c.getResult();
                              if (file.existsAsFile())
                                  safe->loadSampleFile (lane, file);
                          });
}

bool HKR5AudioProcessorEditor::WaveView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return firstAudioFile (files).existsAsFile();
}

void HKR5AudioProcessorEditor::WaveView::fileDragEnter (const juce::StringArray&, int, int)
{
    hot = true;
    repaint();
}

void HKR5AudioProcessorEditor::WaveView::fileDragExit (const juce::StringArray&)
{
    hot = false;
    repaint();
}

void HKR5AudioProcessorEditor::WaveView::filesDropped (const juce::StringArray& files, int, int)
{
    hot = false;
    repaint();
    const auto file = firstAudioFile (files);
    if (editor != nullptr && editor->editLane >= 0 && file.existsAsFile())
        editor->loadSampleFile (editor->editLane, file);
}

void HKR5AudioProcessorEditor::WaveView::paint (juce::Graphics& g)
{
    auto box = getLocalBounds().toFloat();
    g.setColour (kDark);
    g.fillRoundedRectangle (box, 4.f);
    g.setColour (hot ? kCyan : kMetal.withAlpha (0.25f));
    g.drawRoundedRectangle (box.reduced (0.5f), 4.f, hot ? 2.f : 1.f);

    auto inner = box.reduced (10.f, 0.f);
    auto header = inner.removeFromTop (22.f);
    auto axis = inner.removeFromBottom (18.f);
    auto r = inner.reduced (0.f, 4.f);

    g.setFont (boldFont (13.f));
    g.setColour (kWhite);
    g.drawText (title, header, juce::Justification::centredLeft, false);
    g.setColour (kCyan);
    g.drawText (info, header, juce::Justification::centredRight, false);

    const float mid = r.getCentreY();
    const float amp = r.getHeight() * 0.5f;
    g.setColour (kMetal.withAlpha (0.35f));
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    const float g6 = juce::Decibels::decibelsToGain (-6.f) * amp;
    g.setColour (kMetal.withAlpha (0.12f));
    g.drawHorizontalLine ((int) (mid - g6), r.getX(), r.getRight());
    g.drawHorizontalLine ((int) (mid + g6), r.getX(), r.getRight());

    // ms ticks: pick the step that gives roughly 4-8 ticks across the shot.
    const float durMs = durSec * 1000.f;
    float step = 1000.f;
    for (float s : { 1.f, 2.f, 5.f, 10.f, 25.f, 50.f, 100.f, 250.f, 500.f })
        if (durMs / s <= 8.f) { step = s; break; }
    g.setFont (boldFont (10.f));
    for (float t = 0.f; t <= durMs + 0.5f; t += step)
    {
        const float x = r.getX() + t / juce::jmax (1.f, durMs) * r.getWidth();
        g.setColour (kMetal.withAlpha (0.12f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (kMetal.withAlpha (0.7f));
        g.drawText (juce::String (juce::roundToInt (t)) + " ms",
                    juce::Rectangle<float> (x - 30.f, axis.getY(), 60.f, axis.getHeight()),
                    juce::Justification::centred, false);
    }

    const int n = (int) hi.size();
    if (n >= 2 && (int) lo.size() == n)
    {
        auto xAt = [&] (int i) { return r.getX() + (float) i / (float) (n - 1) * r.getWidth(); };
        juce::Path shape;
        shape.startNewSubPath (xAt (0), mid - hi[0] * amp);
        for (int i = 1; i < n; ++i)
            shape.lineTo (xAt (i), mid - hi[(size_t) i] * amp);
        for (int i = n - 1; i >= 0; --i)
            shape.lineTo (xAt (i), mid - lo[(size_t) i] * amp);
        shape.closeSubPath();

        g.setColour (kCyan.withAlpha (0.35f));
        g.fillPath (shape);
        g.setColour (kCyan);
        g.strokePath (shape, juce::PathStrokeType (1.f));
        if (play01 >= 0.f && play01 <= 1.f)
        {
            // Tint what has already sounded.
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (juce::Rectangle<float> (r.getX(), r.getY(),
                                                        play01 * r.getWidth(), r.getHeight()).toNearestInt());
            g.setColour (kMagenta.withAlpha (0.5f));
            g.fillPath (shape);
        }
    }

    g.setFont (boldFont (10.f));
    g.setColour (kMetal.withAlpha (0.8f));
    g.drawText ("peak " + juce::String (peakDb, 1) + " dB",
                r.reduced (4.f).removeFromTop (14.f), juce::Justification::topRight, false);

    if (play01 >= 0.f && play01 <= 1.f)
    {
        const float x = r.getX() + play01 * r.getWidth();
        g.setColour (kMagenta);
        g.drawLine (x, r.getY(), x, r.getBottom(), 2.f);
    }
}

void HKR5AudioProcessorEditor::contentMouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && titleHitBounds().contains (e.getPosition()))
        openAbout();
}

void HKR5AudioProcessorEditor::openAbout()
{
    aboutBox.setVisible (true);
    aboutBox.toFront (true);
}

void HKR5AudioProcessorEditor::cycleLib (bool bank, int delta)
{
    const auto names = bank ? proc.bankNames() : proc.patchNames();
    if (names.isEmpty())
        return;
    const auto cur = bank ? proc.bankName() : proc.patchName();
    int idx = names.indexOf (cur);
    if (idx < 0)
        idx = delta > 0 ? 0 : names.size() - 1;
    else
        idx = (idx + delta + names.size()) % names.size();
    if (bank)
        proc.loadBankByName (names[idx]);
    else
        proc.loadPatchByName (names[idx]);
    refreshBars();
}

void HKR5AudioProcessorEditor::openLibMenu (bool bank)
{
    juce::PopupMenu m;
    const auto names = bank ? proc.bankNames() : proc.patchNames();
    const auto cur = bank ? proc.bankName() : proc.patchName();
    m.addItem (kMenuInit, bank ? "Init Bank" : "Init Patch", true,
               cur == (bank ? "Init Bank" : "Init Patch"));
    m.addSeparator();
    m.addItem (kMenuSave, "Save");
    m.addItem (kMenuSaveAs, "Save As...");
    m.addItem (kMenuExport, "Export...");
    m.addItem (kMenuImport, "Import...");
    if (names.size() > 0)
    {
        m.addSeparator();
        for (int i = 0; i < names.size(); ++i)
            m.addItem (kMenuFiles + i, names[i], true, names[i] == cur);
    }

    juce::Component::SafePointer<HKR5AudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (bank ? (juce::Component*) &bankBar
                                                                          : (juce::Component*) &patchBar),
                     [safe, bank, names] (int result)
                     {
                         if (safe == nullptr || result <= 0)
                             return;
                         if (result == kMenuInit)
                         {
                             if (bank)
                                 safe->proc.initBank();
                             else
                                 safe->proc.initPatch();
                         }
                         else if (result == kMenuSave)
                         {
                             const bool ok = bank ? safe->proc.saveBank() : safe->proc.savePatch();
                             if (! ok)
                                 safe->chooseSaveAs (bank);
                         }
                         else if (result == kMenuSaveAs)
                             safe->chooseSaveAs (bank);
                         else if (result == kMenuExport)
                             safe->chooseExport (bank);
                         else if (result == kMenuImport)
                             safe->chooseImport (bank);
                         else if (result >= kMenuFiles)
                         {
                             const int i = result - kMenuFiles;
                             if (juce::isPositiveAndBelow (i, names.size()))
                             {
                                 if (bank)
                                     safe->proc.loadBankByName (names[i]);
                                 else
                                     safe->proc.loadPatchByName (names[i]);
                             }
                         }
                         safe->refreshBars();
                         safe->refreshGrid();
                     });
}

void HKR5AudioProcessorEditor::chooseSaveAs (bool bank)
{
    const auto ext = bank ? "*.5hkrb" : "*.5hkrp";
    const auto start = (bank ? proc.banksDir() : proc.patchesDir())
                           .getChildFile (bank ? "Bank.5hkrb" : "Patch.5hkrp");
    chooser = std::make_unique<juce::FileChooser> (bank ? "Save Bank" : "Save Patch", start, ext);
    juce::Component::SafePointer<HKR5AudioProcessorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, bank] (const juce::FileChooser& c)
                          {
                              if (safe == nullptr)
                                  return;
                              auto file = c.getResult();
                              if (file.getFullPathName().isEmpty())
                                  return;
                              if (bank)
                                  safe->proc.saveBankAs (file.getFileNameWithoutExtension());
                              else
                                  safe->proc.savePatchAs (file.getFileNameWithoutExtension());
                              safe->refreshBars();
                          });
}

void HKR5AudioProcessorEditor::chooseExport (bool bank)
{
    const auto ext = bank ? "*.5hkrb" : "*.5hkrp";
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile (bank ? "5HKR Bank.5hkrb" : "5HKR Patch.5hkrp");
    chooser = std::make_unique<juce::FileChooser> (bank ? "Export Bank" : "Export Patch", start, ext);
    juce::Component::SafePointer<HKR5AudioProcessorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, bank] (const juce::FileChooser& c)
                          {
                              if (safe == nullptr)
                                  return;
                              auto file = c.getResult();
                              if (file.getFullPathName().isEmpty())
                                  return;
                              if (bank)
                                  safe->proc.exportBank (file);
                              else
                                  safe->proc.exportPatch (file);
                          });
}

void HKR5AudioProcessorEditor::chooseImport (bool bank)
{
    chooser = std::make_unique<juce::FileChooser> (
        bank ? "Import Bank" : "Import Patch",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        bank ? "*.5hkrb" : "*.5hkrp");
    juce::Component::SafePointer<HKR5AudioProcessorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [safe, bank] (const juce::FileChooser& c)
                          {
                              if (safe == nullptr)
                                  return;
                              auto file = c.getResult();
                              if (! file.existsAsFile())
                                  return;
                              if (bank)
                                  safe->proc.importBank (file);
                              else
                                  safe->proc.importPatch (file);
                              safe->refreshBars();
                              safe->refreshGrid();
                          });
}
