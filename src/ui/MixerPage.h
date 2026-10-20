#pragma once

#include "../PluginProcessor.h"

#include <JuceHeader.h>

class MixerPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit MixerPage (HKR5AudioProcessor& p) : proc (p)
    {
        for (int i = 0; i < hkr::numLanes; ++i)
        {
            auto* s = strips.add (new Strip (*this, i));
            addAndMakeVisible (s);
        }
    }

    ~MixerPage() override { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        const int w = juce::jmax (1, r.getWidth() / hkr::numLanes);
        for (int i = 0; i < strips.size(); ++i)
            strips[i]->setBounds (r.removeFromLeft (i == strips.size() - 1 ? r.getWidth() : w));
    }

    HKR5AudioProcessor& proc;

private:
    void visibilityChanged() override
    {
        if (isVisible())
            startTimerHz (24);
        else
            stopTimer();
    }

    void timerCallback() override
    {
        for (auto* s : strips)
            s->tickMeter();
    }

    static const juce::Colour kCyan;
    static const juce::Colour kMagenta;
    static const juce::Colour kMetal;
    static const juce::Colour kWhite;
    static const juce::Colour kDark;
    static const juce::Colour kLane;
    static const juce::Colour kAccent[hkr::numLanes];

    static juce::Font labelFont (float h)
    {
        return juce::Font (juce::FontOptions (h).withStyle ("Bold"));
    }

    struct MixLnF : public juce::LookAndFeel_V4
    {
        juce::Colour accent { kCyan };

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override
        {
            auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
            const float d = juce::jmin (bounds.getWidth(), bounds.getHeight());
            auto rc = bounds.withSizeKeepingCentre (d, d);
            g.setColour (kDark);
            g.fillEllipse (rc);
            auto cap = rc.reduced (d * 0.18f);
            g.setColour (juce::Colour (0xff3a4450));
            g.fillEllipse (cap);
            const float angle = startAngle + pos * (endAngle - startAngle);
            const auto c = cap.getCentre();
            const auto needle = c.getPointOnCircumference (cap.getWidth() * 0.42f, angle);
            g.setColour (accent);
            g.drawLine (c.x, c.y, needle.x, needle.y, 2.f);
            g.fillEllipse (needle.x - 2.f, needle.y - 2.f, 4.f, 4.f);
        }

        void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                               juce::Slider::SliderStyle, juce::Slider&) override
        {
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.f);
            const bool on = b.getToggleState();
            g.setColour (on ? kMagenta.withAlpha (0.85f) : kDark);
            g.fillRoundedRectangle (r, 3.f);
            g.setColour (on ? kWhite : kCyan.withAlpha (0.55f));
            if (b.getComponentID() == "mute")
            {
                juce::Path p;
                const float cx = r.getCentreX() - 2.f, cy = r.getCentreY();
                p.addRectangle (cx - 6.f, cy - 3.f, 4.f, 6.f);
                p.addTriangle (cx - 2.f, cy - 5.f, cx + 4.f, cy, cx - 2.f, cy + 5.f);
                g.fillPath (p);
            }
            else
            {
                g.setFont (labelFont (11.f));
                g.drawText ("S", r.toNearestInt(), juce::Justification::centred, false);
            }
        }
    };

    struct MixKnob : public juce::Component
    {
        MixKnob (HKR5AudioProcessor& st, const char* id, const juce::String& name, juce::Colour accent)
        {
            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (labelFont (9.f));
            label.setColour (juce::Label::textColourId, kMetal);
            label.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (label);
            lnf.accent = accent;
            slider.setLookAndFeel (&lnf);
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            slider.setOpaque (false);
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (st.apvts.getParameter (id)))
                slider.setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                st.apvts, id, slider);
            addAndMakeVisible (slider);
        }

        ~MixKnob() override { slider.setLookAndFeel (nullptr); }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromBottom (11));
            slider.setBounds (r);
        }

        MixLnF lnf;
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    struct Strip : public juce::Component
    {
        Strip (MixerPage& page, int ln)
            : proc (page.proc), lane (ln), accent (kAccent[ln]),
              gain (page.proc, ParamID::laneMixGain[ln], "Gain", accent),
              high (page.proc, ParamID::laneEqHi[ln], "High", accent),
              mid (page.proc, ParamID::laneEqMid[ln], "Mid", accent),
              low (page.proc, ParamID::laneEqLo[ln], "Low", accent),
              filter (page.proc, ParamID::laneFilter[ln], "Filter", accent),
              pan (page.proc, ParamID::lanePan[ln], "Pan", accent)
        {
            juce::Component* knobs[] = { &gain, &high, &mid, &low, &filter, &pan };
            for (auto* k : knobs)
                addAndMakeVisible (k);

            mute.setComponentID ("mute");
            mute.setLookAndFeel (&lnf);
            muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                page.proc.apvts, ParamID::laneOn[ln], mute);
            addAndMakeVisible (mute);

            solo.setComponentID ("solo");
            solo.setLookAndFeel (&lnf);
            soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                page.proc.apvts, ParamID::laneSolo[ln], solo);
            addAndMakeVisible (solo);

            fader.setLookAndFeel (&lnf);
            fader.setSliderStyle (juce::Slider::LinearVertical);
            fader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            fader.setOpaque (false);
            fader.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
            fader.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (
                    page.proc.apvts.getParameter (ParamID::laneFader[ln])))
                fader.setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
            faderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                page.proc.apvts, ParamID::laneFader[ln], fader);
            fader.onValueChange = [this] { repaint(); };
            addAndMakeVisible (fader);

            name.setText (hkr::laneNames[ln], juce::dontSendNotification);
            name.setJustificationType (juce::Justification::centred);
            name.setFont (labelFont (11.f));
            name.setColour (juce::Label::textColourId, kWhite);
            name.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (name);
        }

        ~Strip() override
        {
            mute.setLookAndFeel (nullptr);
            solo.setLookAndFeel (nullptr);
            fader.setLookAndFeel (nullptr);
        }

        void tickMeter()
        {
            const float p = proc.lanePeak (lane);
            if (std::abs (p - shown) < 0.002f)
                return;
            shown = p;
            repaint();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (3, 2);
            name.setBounds (r.removeFromBottom (16));
            auto btns = r.removeFromBottom (22);
            mute.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (2, 1));
            solo.setBounds (btns.reduced (2, 1));
            const int knobH = juce::jmin (58, juce::jmax (36, r.getHeight() / 9));
            juce::Component* knobs[] = { &gain, &high, &mid, &low, &filter, &pan };
            for (auto* k : knobs)
                k->setBounds (r.removeFromTop (knobH));
            fader.setBounds (r.reduced (4, 2));
        }

        void paint (juce::Graphics& g) override
        {
            auto body = getLocalBounds().toFloat().reduced (1.f);
            g.setColour (kLane);
            g.fillRoundedRectangle (body, 4.f);
            g.setColour (accent.withAlpha (0.35f));
            g.drawRoundedRectangle (body.reduced (0.5f), 4.f, 1.f);

            auto area = fader.getBounds().toFloat().reduced (0.f, 6.f);
            auto meter = area.withSizeKeepingCentre (8.f, area.getHeight());
            g.setColour (kDark);
            g.fillRoundedRectangle (meter, 2.f);
            const float pk = hkr::meterToPos (shown);
            if (pk > 0.001f)
            {
                auto fill = meter.removeFromBottom (juce::jmax (2.f, meter.getHeight() * pk));
                g.setColour (shown >= 1.f ? kMagenta : accent.withAlpha (0.9f));
                g.fillRoundedRectangle (fill, 2.f);
            }

            const float pos = (float) fader.valueToProportionOfLength (fader.getValue());
            const float hy = area.getBottom() - pos * area.getHeight();
            auto handle = juce::Rectangle<float> (area.getCentreX() - 18.f, hy - 6.f, 36.f, 12.f);
            g.setColour (juce::Colour (0xff2a3340));
            g.fillRoundedRectangle (handle, 2.f);
            g.setColour (accent);
            g.drawRoundedRectangle (handle.reduced (0.5f), 2.f, 1.4f);
            g.drawLine (handle.getX() + 5.f, handle.getCentreY(),
                        handle.getRight() - 5.f, handle.getCentreY(), 1.2f);
        }

        HKR5AudioProcessor& proc;
        int lane = 0;
        float shown = 0.f;
        juce::Colour accent;
        MixLnF lnf;
        MixKnob gain, high, mid, low, filter, pan;
        juce::ToggleButton mute, solo;
        juce::Slider fader;
        juce::Label name;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt, soloAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAtt;
    };

    juce::OwnedArray<Strip> strips;
};

inline const juce::Colour MixerPage::kCyan { 0xff00ffff };
inline const juce::Colour MixerPage::kMagenta { 0xffff00ff };
inline const juce::Colour MixerPage::kMetal { 0xffc8d6e5 };
inline const juce::Colour MixerPage::kWhite { 0xffffffff };
inline const juce::Colour MixerPage::kDark { 0xff000000 };
inline const juce::Colour MixerPage::kLane { 0xff121a26 };
inline const juce::Colour MixerPage::kAccent[hkr::numLanes] {
    juce::Colour (0xffff3b3b), juce::Colour (0xffffcc00),
    juce::Colour (0xff4d8cff), juce::Colour (0xffff5a1a),
    juce::Colour (0xffffaa00), juce::Colour (0xff3bff6a),
    juce::Colour (0xffff8a00), juce::Colour (0xff3ba9ff)
};
