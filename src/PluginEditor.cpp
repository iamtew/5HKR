#include "PluginEditor.h"

namespace
{
constexpr int kEditorW = 980;
constexpr int kEditorH = 560;
constexpr const char* kPageNames[] = { "Main", "Sidechain" };
constexpr int kPageCount = 2;
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
} // namespace

HKR5AudioProcessorEditor::Knob::Knob (juce::AudioProcessorValueTreeState& st,
                                      const juce::String& id, const juce::String& name)
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
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
}

void HKR5AudioProcessorEditor::Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (kLabelH));
    slider.setBounds (r);
}

HKR5AudioProcessorEditor::LaneRow::LaneRow (HKR5AudioProcessorEditor& ed, int ln)
    : editor (ed), lane (ln)
{
    onBtn.setButtonText (hkr::laneNames[ln]);
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        ed.proc.apvts, ParamID::laneOn[ln], onBtn);
    addAndMakeVisible (onBtn);

    swing.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    swing.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        ed.proc.apvts, ParamID::laneSwing[ln], swing);
    addAndMakeVisible (swing);

    tune.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    tune.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    tuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        ed.proc.apvts, ParamID::laneTune[ln], tune);
    addAndMakeVisible (tune);
}

void HKR5AudioProcessorEditor::LaneRow::resized()
{
    auto r = getLocalBounds();
    onBtn.setBounds (r.removeFromLeft (56).reduced (1));
    swing.setBounds (r.removeFromLeft (40).reduced (1));
    tune.setBounds (r.removeFromLeft (40).reduced (1));
    const int cellW = juce::jmax (1, r.getWidth() / 16);
    for (int i = 0; i < 16; ++i)
        cells[i] = { r.getX() + i * cellW, r.getY() + 4, cellW - 2, r.getHeight() - 8 };
}

void HKR5AudioProcessorEditor::LaneRow::paint (juce::Graphics& g)
{
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
    for (int i = 0; i < 16; ++i)
    {
        if (! cells[i].contains (e.getPosition()))
            continue;
        const int idx = editor.stepPage * 16 + i;
        if (e.mods.isPopupMenu())
            editor.proc.toggleLoopEnd (lane, idx);
        else
            values[i] = editor.proc.cycleStep (lane, idx);
        editor.refreshGrid();
        return;
    }
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

    const float peak = juce::jlimit (0.f, 1.f, editor.proc.outputMeter().lastPeak());
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

HKR5AudioProcessorEditor::HKR5AudioProcessorEditor (HKR5AudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
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

    for (int i = 0; i < hkr::numLanes; ++i)
    {
        auto* row = rows.add (new LaneRow (*this, i));
        content.addAndMakeVisible (row);
    }

    scOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, ParamID::scOn, scOn);
    content.addAndMakeVisible (scOn);
    struct Spec { const char* id; const char* name; };
    static constexpr Spec kSc[] = {
        { ParamID::scThresh, "Thresh" }, { ParamID::scAmount, "Amount" },
        { ParamID::scAttack, "Attack" }, { ParamID::scRelease, "Release" },
        { ParamID::scMix, "Mix" }
    };
    for (auto& s : kSc)
    {
        auto* k = scKnobs.add (new Knob (proc.apvts, s.id, s.name));
        content.addAndMakeVisible (k);
    }

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
    pageBar.setBounds (title.removeFromLeft (88).reduced (4, 4));
    const int barW = title.getWidth() / 2;
    patchBar.setBounds (title.removeFromLeft (barW).reduced (6, 2));
    bankBar.setBounds (title.reduced (6, 2));

    auto tools = r.removeFromTop (28);
    bankBox.setBounds (tools.removeFromLeft (90).reduced (2, 2));
    patBox.setBounds (tools.removeFromLeft (90).reduced (2, 2));
    lenBox.setBounds (tools.removeFromLeft (90).reduced (2, 2));
    pageLeft.setBounds (tools.removeFromLeft (28).reduced (2, 2));
    pageLabel.setBounds (tools.removeFromLeft (72));
    pageRight.setBounds (tools.removeFromLeft (28).reduced (2, 2));

    auto sc = r;
    scOn.setBounds (sc.removeFromTop (28).removeFromLeft (110));
    const int kw = juce::jmax (1, sc.getWidth() / juce::jmax (1, scKnobs.size()));
    for (int i = 0; i < scKnobs.size(); ++i)
        scKnobs[i]->setBounds (sc.getX() + i * kw, sc.getY(), kw, juce::jmin (140, sc.getHeight()));

    const int rowH = juce::jmax (1, r.getHeight() / hkr::numLanes);
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (r.getX(), r.getY() + i * rowH, r.getWidth(), rowH);

    aboutBox.setBounds (content.getLocalBounds());
    applyPageVisibility();
}

void HKR5AudioProcessorEditor::timerCallback()
{
    volumeStrip.refresh();
    refreshBars();
    refreshGrid();
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
    pageBar.setText (kPageNames[editorPage]);
    applyPageVisibility();
}

void HKR5AudioProcessorEditor::applyPageVisibility()
{
    const bool main = editorPage == 0;
    bankBox.setVisible (main);
    patBox.setVisible (main);
    lenBox.setVisible (main);
    pageLeft.setVisible (main);
    pageRight.setVisible (main);
    pageLabel.setVisible (main);
    for (auto* row : rows)
        row->setVisible (main);
    scOn.setVisible (! main);
    for (auto* k : scKnobs)
        k->setVisible (! main);
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
