#pragma once

#include "PluginProcessor.h"
#include "ui/EffectsPage.h"
#include "ui/EqPage.h"

#include <functional>
#include <vector>

class HKR5AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::DragAndDropContainer,
                                 private juce::Timer
{
public:
    explicit HKR5AudioProcessorEditor (HKR5AudioProcessor&);
    ~HKR5AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Content : public juce::Component
    {
        explicit Content (HKR5AudioProcessorEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintContent (g); }
        void resized() override { ed.layoutContent(); }
        void mouseUp (const juce::MouseEvent& e) override { ed.contentMouseUp (e); }
        HKR5AudioProcessorEditor& ed;
    };
    void paintContent (juce::Graphics&);
    void layoutContent();
    void contentMouseUp (const juce::MouseEvent&);
    void timerCallback() override;
    juce::Rectangle<int> titleHitBounds() const;
    void openAbout();
    void refreshBars();
    void openLibMenu (bool bank);
    void cycleLib (bool bank, int delta);
    void chooseSaveAs (bool bank);
    void chooseExport (bool bank);
    void chooseImport (bool bank);
    void setEditorPage (int page);
    void applyPageVisibility();
    void refreshGrid();

    HKR5AudioProcessor& proc;

    struct PanelLnF : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
        void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
    };

    struct SlotBar : public juce::Component
    {
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void setText (const juce::String& t);
        juce::String text;
        std::function<void (int)> onStep;
        std::function<void()> onOpen;
    };

    struct VolumeStrip : public juce::Component
    {
        explicit VolumeStrip (HKR5AudioProcessorEditor& ed);
        ~VolumeStrip() override;
        void resized() override;
        void paint (juce::Graphics& g) override;
        void refresh();
        juce::Rectangle<float> faderArea() const;
        juce::Rectangle<float> meterArea() const;

        struct LnF : public juce::LookAndFeel_V4
        {
            void drawLinearSlider (juce::Graphics&, int, int, int, int, float pos,
                                   float, float, juce::Slider::SliderStyle, juce::Slider&) override;
        };

        HKR5AudioProcessorEditor& editor;
        LnF lnf;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        bool clipped = false;
        juce::uint32 clipUntil = 0;
    };

    struct Knob : public juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& name);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    struct LaneRow : public juce::Component
    {
        LaneRow (HKR5AudioProcessorEditor& ed, int lane);
        void resized() override;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void refresh (const hkr::SlotData& slot, int playhead, int page);
        void refreshKnobLabels();

        HKR5AudioProcessorEditor& editor;
        int lane = 0;
        juce::ToggleButton onBtn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        juce::ComboBox kitBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> kitAtt;
        juce::Slider swing;
        juce::Label swingLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAtt;
        juce::Slider tune;
        juce::Label tuneLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAtt;
        juce::Slider send;
        juce::Label sendLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sendAtt;
        juce::Rectangle<int> cells[16];
        int values[16] {};
        int play = -1;
        int loopCell = -1;
    };

    struct AboutBox : public juce::Component
    {
        AboutBox();
        void paint (juce::Graphics& g) override;
        void resized() override;
        juce::ToggleButton closeBtn { "Close" };
    };

    // Filtered sidechain key, log Hz. HP/LP lines are markers, not handles.
    struct ScSpec : public juce::Component
    {
        explicit ScSpec (HKR5AudioProcessor& p) : proc (p) {}
        void paint (juce::Graphics& g) override;
        HKR5AudioProcessor& proc;
        std::vector<float> specDb;
    };

    PanelLnF panelLnF;
    Content content { *this };
    int baseW = 0, baseH = 0;
    SlotBar pageBar, patchBar, bankBar;
    VolumeStrip volumeStrip { *this };
    juce::ComboBox lenBox, bankBox, patBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bankAtt, patAtt;
    juce::OwnedArray<LaneRow> rows;
    juce::OwnedArray<Knob> scKnobs;
    EffectsPage effectsPage;
    EqPage eqPage;
    juce::ToggleButton scOn { "Sidechain" };
    juce::ToggleButton scListen { "Listen" };
    ScSpec scSpec { proc };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scOnAtt, scListenAtt;
    juce::TextButton pageLeft { "<" }, pageRight { ">" };
    juce::TextButton randomBtn { "Random" }, clearBtn { "Clear" };
    juce::Label pageLabel;
    AboutBox aboutBox;
    int editorPage = 0;
    int stepPage = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Image logo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HKR5AudioProcessorEditor)
};
