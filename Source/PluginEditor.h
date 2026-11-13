#pragma once
#include "PluginProcessor.h"
#include "Presets.h"
#include "UI/Theme.h"
#include "UI/Knob.h"
#include "UI/BandDisplay.h"
#include "UI/BandPanel.h"
#include "UI/ModPanel.h"

class AnjisEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
{
public:
    explicit AnjisEditor (AnjisAudioProcessor&);
    ~AnjisEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int kW = 1100, kH = 800;
    void testTick() { tick(); }
    void testOpenPicker() { bandPanel.onOpenStylePicker (proc.selectedBand.load()); }
    anjis::ui::ModPanel& getModPanel() { return modPanel; }

private:
    struct Content : public juce::Component
    {
        AnjisEditor& ed;
        explicit Content (AnjisEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintContent (g); }
    };
    struct Scrim : public juce::Component
    {
        std::function<void()> onClick;
        float alpha = 0;
        void paint (juce::Graphics& g) override { g.fillAll (anjis::ui::col::bgDeep.withAlpha (alpha * 0.35f)); }
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };

    void paintContent (juce::Graphics&);
    void layout();
    void tick();
    void tickChildren (juce::Component&);
    void showPresetMenu();
    void showBandMenu (int band);
    void showSizeMenu();
    void savePresetDialog();
    void switchAB (int slot);
    void updateHelp();

    AnjisAudioProcessor& proc;
    anjis::ui::LookAndFeel lnf;
    PresetManager presets;
    Content content { *this };

    anjis::ui::BandDisplay display;
    anjis::ui::BandPanel bandPanel;
    anjis::ui::ModPanel modPanel;
    Scrim scrim;
    anjis::ui::StylePicker picker;

    juce::TextButton prevBtn, nextBtn, presetBtn, saveBtn, undoBtn, redoBtn, aBtn, bBtn, copyAbBtn, sizeBtn;
    anjis::ui::Knob inKnob, mixKnob, outKnob;
    anjis::ui::Choice quality, phase;
    anjis::ui::Toggle autoGain;

    juce::ValueTree bandClipboard;
    juce::String helpText, lastHelp;
    juce::Rectangle<int> helpArea, latencyArea;
    std::unique_ptr<juce::VBlankAttachment> vblank;
    std::unique_ptr<juce::AlertWindow> alert;
    float scale = 1.0f, phaseAnim = 0.0f;
    int lastLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnjisEditor)
};
