#pragma once
#include <JuceHeader.h>

class AnjisAudioProcessor;

// Factory + user presets, plus per-band presets.
class PresetManager
{
public:
    explicit PresetManager (AnjisAudioProcessor& p) : proc (p) {}

    struct Entry { juce::String name, category; bool factory = true; juce::File file; };

    juce::Array<Entry> list() const;
    void load (const Entry& e);
    void loadIndex (int i);
    void step (int delta);
    void initDefaults();
    bool saveUser (const juce::String& name);
    void saveBandPreset (int band, const juce::String& name);
    void loadBandPreset (const juce::File& f, int band);
    juce::Array<juce::File> bandPresets() const;

    static juce::File userDir();
    static juce::File bandDir();

    juce::String getCurrentName() const;
    void setCurrentName (const juce::String& n);
    int currentIndex = 0;

private:
    AnjisAudioProcessor& proc;
};
