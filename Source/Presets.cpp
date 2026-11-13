#include "Presets.h"
#include "PluginProcessor.h"

using namespace anjis;

namespace
{
struct Factory { const char* name; const char* category; std::function<void (AnjisAudioProcessor&)> apply; };

int styleIndex (const char* name) { return styleNames().indexOf (name); }

void band (AnjisAudioProcessor& p, int b, const char* style, float drive, float mix = 100, float lvl = 0)
{
    p.set (bandId (b, "style"), (float) styleIndex (style));
    p.set (bandId (b, "drive"), drive);
    p.set (bandId (b, "mix"), mix);
    p.set (bandId (b, "lvl"), lvl);
}
void route (AnjisAudioProcessor& p, int slot, Src src, int target, float amt)
{
    p.set (slotId (slot, "src"), (float) (int) src);
    p.set (slotId (slot, "dst"), (float) target);
    p.set (slotId (slot, "amt"), amt);
}
int tgt (int b, int which) { return 1 + b * kBandTargets + which; }

const std::vector<Factory>& factory()
{
    static const std::vector<Factory> f {
        { "Init", "Basics", [] (AnjisAudioProcessor&) {} },
        { "Gentle Bus Glue", "Mix", [] (AnjisAudioProcessor& p) { band (p, 0, "Console", 30); } },
        { "Tape Kiss", "Mix", [] (AnjisAudioProcessor& p) { band (p, 0, "Warm Tape", 35); p.set (bandId (0, "tone"), -10); } },
        { "Master Sheen", "Mix", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 3); p.set ("phase", 1); p.set (xoverId (0), 180); p.set (xoverId (1), 5000);
            band (p, 0, "Iron", 18); band (p, 1, "Warm Tape", 20); band (p, 2, "Exciter", 35); p.set ("mix", 60); } },
        { "Hot Tape Drums", "Drums", [] (AnjisAudioProcessor& p) { band (p, 0, "Hot Tape", 60); p.set (bandId (0, "dyn"), 25); } },
        { "Transient Crunch", "Drums", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Smash", 30); p.set (efId (0, "trans"), 1); p.set (efId (0, "gain"), 6);
            route (p, 0, Src::Ef1, tgt (0, 0), 45); } },
        { "Drum Room Smash", "Drums", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 2); p.set (xoverId (0), 250); band (p, 0, "Heavy Iron", 35); band (p, 1, "Smash", 65); p.set ("mix", 70); } },
        { "Crunch Guitar", "Guitar", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 2); p.set (xoverId (0), 300); band (p, 0, "Clean Tube", 15); band (p, 1, "Crunch Amp", 55); } },
        { "Clean Sparkle", "Guitar", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 2); p.set (xoverId (0), 3000); band (p, 0, "Clean Tube", 10); band (p, 1, "Exciter", 60); } },
        { "Edge of Breakup", "Guitar", [] (AnjisAudioProcessor& p) { band (p, 0, "Clean Amp", 40); p.set (bandId (0, "dyn"), 30); } },
        { "Fuzz Wall", "Guitar", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Fuzz", 70); p.set (bandId (0, "fb"), 20); p.set (bandId (0, "fbf"), 800); } },
        { "Bass Growl", "Bass", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 3); p.set (xoverId (0), 120); p.set (xoverId (1), 2500);
            band (p, 0, "Iron", 30); band (p, 1, "Lead Amp", 50, 60); band (p, 2, "Soft Clip", 20, 100, -6); } },
        { "Sub Maker", "Bass", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 2); p.set (xoverId (0), 200); band (p, 0, "Octave Down", 50, 60); } },
        { "Vocal Air", "Vocals", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 2); p.set (xoverId (0), 6000); band (p, 0, "Clean Tube", 5); band (p, 1, "Exciter", 55); } },
        { "Radio Speaker", "Creative", [] (AnjisAudioProcessor& p) {
            p.set ("bands", 3); p.set (xoverId (0), 400); p.set (xoverId (1), 3500);
            p.set (bandId (0, "mute"), 1); p.set (bandId (2, "mute"), 1); band (p, 1, "Broken Speaker", 50); } },
        { "Cassette Memories", "Creative", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Cassette", 50); p.set (lfoId (0, "shape"), 6); p.set (lfoId (0, "rate"), 0.3f);
            route (p, 0, Src::Lfo1, tgt (0, 4), 20); } },
        { "Pumping Drive", "Creative", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Warm Tube", 40); p.set (lfoId (0, "sync"), 1); p.set (lfoId (0, "div"), 7); p.set (lfoId (0, "shape"), 3);
            route (p, 0, Src::Lfo1, tgt (0, 0), 40); } },
        { "Bit Groove", "Creative", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Bitcrush", 50); p.set (lfoId (0, "sync"), 1); p.set (lfoId (0, "div"), 13); p.set (lfoId (0, "shape"), 7);
            const float st[] = { 1, -0.4f, 0.2f, -1, 0.7f, -0.2f, 0.5f, -0.8f };
            for (int i = 0; i < 8; ++i) p.set (lfoStepId (0, i), st[i]);
            route (p, 0, Src::Lfo1, tgt (0, 0), 35); } },
        { "Resonant Feedback", "Creative", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Hot Tube", 50); p.set (bandId (0, "fb"), 60); p.set (bandId (0, "fbf"), 400);
            p.set (lfoId (0, "rate"), 0.2f); route (p, 0, Src::Lfo1, tgt (0, 2), 30); } },
        { "Side Grit", "Creative", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Crunch Amp", 45); p.set (bandId (0, "ch"), 2); p.set (bandId (0, "width"), 130); } },
        { "Wavefold Synth", "Creative", [] (AnjisAudioProcessor& p) {
            band (p, 0, "Sine Fold", 55); route (p, 0, Src::Eg1, tgt (0, 0), 40); p.set (egId (0, "trig"), 1); } },
    };
    return f;
}

const char* bandSuffixes[] = { "style", "drive", "fb", "fbf", "dyn", "tone", "mix", "lvl", "width", "ch" };
}

juce::File PresetManager::userDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("Anji").getChildFile ("Anjis Saturator").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::File PresetManager::bandDir()
{
    auto d = userDir().getParentDirectory().getChildFile ("Band Presets");
    d.createDirectory();
    return d;
}

juce::String PresetManager::getCurrentName() const { return proc.apvts.state.getProperty ("presetName", "Init").toString(); }
void PresetManager::setCurrentName (const juce::String& n)
{
    proc.apvts.state.setProperty ("presetName", n, nullptr);
    auto l = list();
    for (int i = 0; i < l.size(); ++i) if (l[i].name == n) currentIndex = i;
}

juce::Array<PresetManager::Entry> PresetManager::list() const
{
    juce::Array<Entry> r;
    for (auto& f : factory()) r.add ({ f.name, f.category, true, {} });
    auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.anjis");
    files.sort();
    for (auto& f : files) r.add ({ f.getFileNameWithoutExtension(), "User", false, f });
    return r;
}

void PresetManager::initDefaults()
{
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void PresetManager::load (const Entry& e)
{
    proc.undo.beginNewTransaction ("Load preset");
    if (e.factory)
    {
        initDefaults();
        for (auto& f : factory()) if (e.name == f.name) f.apply (proc);
    }
    else if (auto xml = juce::XmlDocument::parse (e.file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (proc.apvts.state.getType())) proc.apvts.replaceState (tree);
    }
    setCurrentName (e.name);
    proc.selectedBand = 0;
}

void PresetManager::loadIndex (int i)
{
    auto l = list();
    if (l.isEmpty()) return;
    currentIndex = (i % l.size() + l.size()) % l.size();
    load (l[currentIndex]);
}

void PresetManager::step (int delta) { loadIndex (currentIndex + delta); }

bool PresetManager::saveUser (const juce::String& name)
{
    auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return false;
    auto f = userDir().getChildFile (safe + ".anjis");
    if (auto xml = proc.apvts.copyState().createXml())
        if (xml->writeTo (f))
        {
            setCurrentName (safe);
            auto l = list();
            for (int i = 0; i < l.size(); ++i) if (l[i].file == f) currentIndex = i;
            return true;
        }
    return false;
}

void PresetManager::saveBandPreset (int b, const juce::String& name)
{
    auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return;
    juce::XmlElement x ("ANJISBAND");
    for (auto* s : bandSuffixes) x.setAttribute (s, proc.get (bandId (b, s)));
    x.writeTo (bandDir().getChildFile (safe + ".anjisband"));
}

void PresetManager::loadBandPreset (const juce::File& f, int b)
{
    if (auto x = juce::XmlDocument::parse (f))
    {
        proc.undo.beginNewTransaction ("Load band preset");
        for (auto* s : bandSuffixes)
            if (x->hasAttribute (s)) proc.set (bandId (b, s), (float) x->getDoubleAttribute (s));
    }
}

juce::Array<juce::File> PresetManager::bandPresets() const
{
    auto files = bandDir().findChildFiles (juce::File::findFiles, false, "*.anjisband");
    files.sort();
    return files;
}
