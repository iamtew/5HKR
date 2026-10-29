#pragma once

#include "PluginProcessor.h"
#include "ui/EffectsPage.h"
#include "ui/EqPage.h"
#include "ui/MixerPage.h"

#include <functional>
#include <vector>

class HKR5AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::DragAndDropContainer,
                                 private juce::Timer,
                                 private juce::AudioProcessorValueTreeState::Listener
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
    void parameterChanged (const juce::String&, float) override;
    void openSoundEdit (int lane);
    void leaveSoundEdit();
    void bindEditLane();
    void rebuildWave();
    void rebuildSampleWave();
    void listenLaneParams (bool on);
    void loadSampleFile (int lane, const juce::File& file);
    void chooseLaneSample();
    bool laneIsSample (int lane) const;

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
        explicit Knob (const juce::String& name);
        void attach (juce::AudioProcessorValueTreeState& st, const juce::String& id);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        int labelH = 12;
    };

    struct WaveView : public juce::Component,
                      public juce::FileDragAndDropTarget
    {
        void paint (juce::Graphics& g) override;
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray&, int, int) override;
        void fileDragExit (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray& files, int, int) override;
        HKR5AudioProcessorEditor* editor = nullptr;
        bool hot = false;
        std::vector<float> lo, hi; // per-bucket min/max, normalised to the shot's peak
        float durSec = 1.f;
        float play01 = -1.f;
        float peakDb = -100.f;
        float atk01 = -1.f; // <0: no fade overlay (synth edit)
        float rel01 = -1.f;
        bool rev = false;
        juce::String title, info;
    };

    struct LaneRow : public juce::Component,
                     public juce::FileDragAndDropTarget
    {
        LaneRow (HKR5AudioProcessorEditor& ed, int lane);
        void resized() override;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray&, int, int) override;
        void fileDragExit (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray& files, int, int) override;
        void refresh (const hkr::SlotData& slot, int playhead, int page);
        void refreshKnobLabels();

        HKR5AudioProcessorEditor& editor;
        int lane = 0;
        juce::Label name;
        juce::TextButton previewBtn { "Play" };
        juce::ToggleButton mute { "M" }, solo { "S" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt, soloAtt;
        juce::ComboBox kitBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> kitAtt;
        juce::TextButton editBtn { "Edit" };
        bool compact = false; // edit mode: only name, kit and Edit
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
        bool hot = false;
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
    MixerPage mixerPage;
    EffectsPage effectsPage;
    EqPage eqPage;
    juce::ToggleButton scOn { "Sidechain" };
    juce::ToggleButton scListen { "Listen" };
    ScSpec scSpec { proc };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scOnAtt, scListenAtt;
    juce::TextButton pageLeft { "<" }, pageRight { ">" };
    juce::TextButton playBtn { "Play" };
    juce::TextButton randomBtn { "Random" }, clearBtn { "Clear" };
    juce::Label pageLabel;
    WaveView wave;
    juce::OwnedArray<Knob> editKnobs;
    juce::TextButton editLoad { "Load" };
    juce::TextButton editUnload { "Unload" };
    juce::ToggleButton editReverse { "Reverse" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> editRevAtt;
    MixerPage::Strip editStrip { proc, 0 };
    AboutBox aboutBox;
    int editorPage = 0;
    int stepPage = 0;
    int editLane = -1;
    bool editIsSample = false;
    bool bindingEdit = false;
    bool waveDirty = true;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Image logo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HKR5AudioProcessorEditor)
};
