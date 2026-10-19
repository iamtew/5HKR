#pragma once

#include "../PluginProcessor.h"

#include <JuceHeader.h>

class EffectsPage : public juce::Component
{
public:
    static constexpr int kRailW = 110;
    static constexpr int kRowH = 96;
    static constexpr int kStripW = 72;

    explicit EffectsPage (HKR5AudioProcessor& p) : proc (p)
    {
        viewport.setScrollBarsShown (true, false);
        viewport.setViewedComponent (&stack, false);
        addAndMakeVisible (viewport);

        for (int t = 0; t < hkr::fxCount; ++t)
        {
            auto* rail = rails.add (new RailButton (*this, t));
            addAndMakeVisible (rail);
            auto* row = rows.add (new FxRow (*this, t));
            stack.addAndMakeVisible (row);
        }
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        auto body = getLocalBounds();
        auto rail = body.removeFromLeft (kRailW);
        g.setColour (kLane);
        g.fillRoundedRectangle (rail.toFloat(), 4.f);
        g.setColour (kCyan.withAlpha (0.35f));
        g.drawRoundedRectangle (rail.toFloat().reduced (0.5f), 4.f, 1.f);

        if (nVisible == 0)
        {
            g.setColour (kMetal);
            g.setFont (juce::Font (juce::FontOptions (13.f).withStyle ("Bold")));
            g.drawText ("Click an effect to add it",
                        body.reduced (12), juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        railArea = r.removeFromLeft (kRailW);
        viewport.setBounds (r.reduced (4, 0));
        layoutRail();
        layoutStack();
    }

    void refresh()
    {
        proc.getFxOrder (railOrder, nRail);
        nRail = hkr::completeFxOrder (railOrder, nRail);

        auto on = [&] (int t)
        {
            auto* v = proc.apvts.getRawParameterValue (ParamID::fxOnIds[t]);
            return v != nullptr && v->load() >= 0.5f;
        };

        nVisible = 0;
        for (int t = 0; t < hkr::fxCount; ++t)
        {
            rails[t]->setOn (on (t));
            rows[t]->setVisible (false);
        }
        for (int i = 0; i < nRail; ++i)
        {
            const int t = railOrder[i];
            if (! on (t))
                continue;
            rows[t]->setVisible (true);
            rows[t]->toFront (false);
            rows[t]->syncDelay();
            visOrder[nVisible++] = t;
        }
        layoutRail();
        layoutStack();
        viewport.setVisible (nVisible > 0);
        repaint();
    }

private:
    static const juce::Colour kCyan;
    static const juce::Colour kMagenta;
    static const juce::Colour kMetal;
    static const juce::Colour kWhite;
    static const juce::Colour kKnob;
    static const juce::Colour kLane;
    static const juce::Colour kLedOn;
    static const juce::Colour kLedOff;

    static juce::Font labelFont (float h)
    {
        return juce::Font (juce::FontOptions (h).withStyle ("Bold"));
    }

    struct FxKnob : public juce::Component
    {
        FxKnob (HKR5AudioProcessor& p, const char* id, const juce::String& name) : proc (p)
        {
            label.setJustificationType (juce::Justification::centred);
            label.setFont (labelFont (11.f));
            label.setColour (juce::Label::textColourId, kCyan);
            label.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (label);
            combo.setColour (juce::ComboBox::textColourId, kWhite);
            combo.setColour (juce::ComboBox::arrowColourId, kCyan);
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 12);
            slider.setColour (juce::Slider::textBoxTextColourId, kWhite);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            bind (id, name);
        }

        void bind (const char* id, const juce::String& name)
        {
            label.setText (name, juce::dontSendNotification);
            sliderAtt.reset();
            btnAtt.reset();
            comboAtt.reset();

            auto* param = proc.apvts.getParameter (id);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
            auto* boolean = dynamic_cast<juce::AudioParameterBool*> (param);

            button.setVisible (false);
            combo.setVisible (false);
            slider.setVisible (false);

            if (boolean != nullptr)
            {
                isBool = true;
                isChoice = false;
                button.setButtonText (name);
                button.setComponentID ("led");
                button.setVisible (true);
                addAndMakeVisible (button);
                btnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    proc.apvts, id, button);
            }
            else if (choice != nullptr)
            {
                isBool = false;
                isChoice = true;
                combo.clear (juce::dontSendNotification);
                for (int i = 0; i < choice->choices.size(); ++i)
                    combo.addItem (choice->choices[i], i + 1);
                combo.setVisible (true);
                addAndMakeVisible (combo);
                comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    proc.apvts, id, combo);
            }
            else if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                isBool = false;
                isChoice = false;
                slider.setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
                slider.setVisible (true);
                addAndMakeVisible (slider);
                sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    proc.apvts, id, slider);
            }
            resized();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (12));
            if (isBool)
                button.setBounds (r.removeFromTop (22).reduced (2, 2));
            else if (isChoice)
                combo.setBounds (r.removeFromTop (22).reduced (2, 2));
            else
                slider.setBounds (r);
        }

        HKR5AudioProcessor& proc;
        juce::Label label;
        juce::Slider slider;
        juce::ToggleButton button;
        juce::ComboBox combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> btnAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
        bool isBool = false;
        bool isChoice = false;
    };

    struct FxRow : public juce::Component
    {
        FxRow (EffectsPage& page, int t) : owner (page), type (t)
        {
            setName (ParamID::fxNames[t]);
            auto add = [this] (const char* id, const char* name)
            {
                knobs.add (new FxKnob (owner.proc, id, name));
                addAndMakeVisible (knobs.getLast());
            };
            switch (type)
            {
                case 0:
                    add (ParamID::fxChoRate, "Rate");
                    add (ParamID::fxChoDepth, "Depth");
                    add (ParamID::fxChoFb, "Fb");
                    add (ParamID::fxChoMix, "Mix");
                    break;
                case 1:
                    add (ParamID::fxDlySync, "Sync");
                    add (ParamID::fxDlyDiv, "Div");
                    add (ParamID::fxDlyTime, "Time");
                    add (ParamID::fxDlyFb, "Fb");
                    add (ParamID::fxDlyCut, "Cut");
                    add (ParamID::fxDlyMix, "Mix");
                    break;
                case 2:
                    add (ParamID::fxFlaRate, "Rate");
                    add (ParamID::fxFlaDepth, "Depth");
                    add (ParamID::fxFlaFb, "Fb");
                    add (ParamID::fxFlaMix, "Mix");
                    break;
                case 3:
                    add (ParamID::fxPhaRate, "Rate");
                    add (ParamID::fxPhaDepth, "Depth");
                    add (ParamID::fxPhaFb, "Fb");
                    add (ParamID::fxPhaCentre, "Centre");
                    add (ParamID::fxPhaMix, "Mix");
                    break;
                case 4:
                    add (ParamID::fxRevSize, "Size");
                    add (ParamID::fxRevDamp, "Damp");
                    add (ParamID::fxRevWidth, "Width");
                    add (ParamID::fxRevMix, "Mix");
                    break;
                default:
                    break;
            }
        }

        void syncDelay()
        {
            if (type != 1 || knobs.size() < 3)
                return;
            auto* v = owner.proc.apvts.getRawParameterValue (ParamID::fxDlySync);
            const bool sync = v != nullptr && v->load() >= 0.5f;
            knobs[1]->setVisible (sync);
            knobs[2]->setVisible (! sync);
            resized();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.f);
            g.setColour (kKnob);
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (kCyan.withAlpha (0.4f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);

            auto strip = r.removeFromLeft ((float) kStripW).reduced (3.f, 4.f);
            g.setColour (juce::Colour (0xff121a26));
            g.fillRoundedRectangle (strip, 3.f);
            g.setColour (kMagenta);
            g.setFont (labelFont (11.f));
            g.drawFittedText (ParamID::fxNames[type], strip.toNearestInt(),
                              juce::Justification::centred, 3);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 4);
            r.removeFromLeft (kStripW);
            int vis = 0;
            for (auto* k : knobs)
                if (k->isVisible())
                    ++vis;
            if (vis <= 0)
                return;
            const int w = r.getWidth() / vis;
            int i = 0;
            for (auto* k : knobs)
            {
                if (! k->isVisible())
                    continue;
                ++i;
                k->setBounds (r.removeFromLeft (i == vis ? r.getWidth() : w));
            }
        }

        EffectsPage& owner;
        int type = 0;
        juce::OwnedArray<FxKnob> knobs;
    };

    struct RailButton : public juce::Component,
                       public juce::DragAndDropTarget
    {
        RailButton (EffectsPage& page, int t) : owner (page), type (t)
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        void setOn (bool v)
        {
            if (on == v)
                return;
            on = v;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (on ? juce::Colour (0xff243044) : kKnob);
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (on ? kCyan : kCyan.withAlpha (0.35f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);

            const float led = 8.f;
            g.setColour (on ? kLedOn : kLedOff);
            g.fillEllipse (r.getX() + 5.f, r.getCentreY() - led * 0.5f, led, led);

            drawIcon (g, juce::Rectangle<float> (r.getRight() - 28.f, r.getY() + 4.f, 22.f, r.getHeight() - 8.f));

            g.setColour (kMetal);
            g.setFont (labelFont (11.f));
            g.drawFittedText (ParamID::fxNames[type],
                              juce::Rectangle<int> ((int) r.getX() + 16, (int) r.getY(),
                                                    (int) r.getWidth() - 42, (int) r.getHeight()),
                              juce::Justification::centredLeft, 2);
        }

        void drawIcon (juce::Graphics& g, juce::Rectangle<float> box)
        {
            g.setColour (kMetal.withAlpha (on ? 1.f : 0.65f));
            juce::Path p;
            const float x = box.getX(), y = box.getY(), w = box.getWidth(), h = box.getHeight();
            switch (type)
            {
                case 0:
                    g.drawEllipse (x, y + h * 0.25f, w * 0.55f, h * 0.5f, 1.4f);
                    g.drawEllipse (x + w * 0.22f, y + h * 0.25f, w * 0.55f, h * 0.5f, 1.4f);
                    g.drawEllipse (x + w * 0.44f, y + h * 0.25f, w * 0.55f, h * 0.5f, 1.4f);
                    break;
                case 1:
                    for (int i = 0; i < 4; ++i)
                        g.fillRect (x + 4.f * (float) i, y + h * 0.2f, 2.f, h * (0.6f - 0.12f * (float) i));
                    break;
                case 2:
                    for (int i = 0; i < 5; ++i)
                        g.drawLine (x + 3.5f * (float) i, y + h * 0.2f, x + 3.5f * (float) i, y + h * 0.8f, 1.2f);
                    break;
                case 3:
                    p.startNewSubPath (x, y + h * 0.5f);
                    p.quadraticTo (x + w * 0.25f, y + 2.f, x + w * 0.5f, y + h * 0.5f);
                    p.quadraticTo (x + w * 0.75f, y + h - 2.f, x + w, y + h * 0.5f);
                    g.strokePath (p, juce::PathStrokeType (1.4f));
                    break;
                case 4:
                    g.drawRect (x + 3.f, y + h * 0.3f, w * 0.55f, h * 0.5f, 1.2f);
                    g.drawLine (x + 3.f, y + h * 0.3f, x + 8.f, y + h * 0.15f, 1.2f);
                    g.drawLine (x + 3.f + w * 0.55f, y + h * 0.3f, x + 8.f + w * 0.55f, y + h * 0.15f, 1.2f);
                    g.drawLine (x + 8.f, y + h * 0.15f, x + 8.f + w * 0.55f, y + h * 0.15f, 1.2f);
                    break;
                default:
                    break;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() > 6)
                if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
                    c->startDragging ("fx:" + juce::String (type), this);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked())
                return;
            auto* v = owner.proc.apvts.getRawParameterValue (ParamID::fxOnIds[type]);
            const bool next = ! (v != nullptr && v->load() >= 0.5f);
            owner.proc.setFxEnabled (type, next);
            owner.refresh();
        }

        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            return d.description.toString().startsWith ("fx:");
        }

        void itemDropped (const SourceDetails& d) override
        {
            const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
            if (from == type)
                return;
            owner.proc.moveFx (from, type);
            owner.refresh();
        }

        EffectsPage& owner;
        int type = 0;
        bool on = false;
    };

    void layoutRail()
    {
        auto rail = railArea;
        const int n = juce::jmax (1, nRail);
        const int h = juce::jmax (1, rail.getHeight() / n);
        for (int i = 0; i < nRail; ++i)
            rails[railOrder[i]]->setBounds (rail.removeFromTop (i == nRail - 1 ? rail.getHeight() : h)
                                                .reduced (3, 2));
    }

    void layoutStack()
    {
        const int w = juce::jmax (1, viewport.getWidth());
        stack.setSize (w, juce::jmax (viewport.getHeight(), nVisible * kRowH));
        int y = 0;
        for (int i = 0; i < nVisible; ++i)
        {
            rows[visOrder[i]]->setBounds (0, y, w, kRowH);
            y += kRowH;
        }
    }

    HKR5AudioProcessor& proc;
    juce::Viewport viewport;
    juce::Component stack;
    juce::OwnedArray<RailButton> rails;
    juce::OwnedArray<FxRow> rows;
    juce::Rectangle<int> railArea;
    int railOrder[hkr::fxCount] {};
    int nRail = 0;
    int visOrder[hkr::fxCount] {};
    int nVisible = 0;
};

inline const juce::Colour EffectsPage::kCyan { 0xff00ffff };
inline const juce::Colour EffectsPage::kMagenta { 0xffff00ff };
inline const juce::Colour EffectsPage::kMetal { 0xffc8d6e5 };
inline const juce::Colour EffectsPage::kWhite { 0xffffffff };
inline const juce::Colour EffectsPage::kKnob { 0xff0d1520 };
inline const juce::Colour EffectsPage::kLane { 0xff121a26 };
inline const juce::Colour EffectsPage::kLedOn { 0xffff00ff };
inline const juce::Colour EffectsPage::kLedOff { 0xff5a1858 };
