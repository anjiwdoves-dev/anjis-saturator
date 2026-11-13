#pragma once
#include <JuceHeader.h>

// Central list of parameter IDs, style names, modulation sources and targets.
namespace anjis
{
constexpr int kMaxBands   = 6;
constexpr int kMaxXovers  = kMaxBands - 1;
constexpr int kNumLfos    = 4;
constexpr int kNumEgs     = 2;
constexpr int kNumEfs     = 2;
constexpr int kNumXy      = 2;
constexpr int kNumMacros  = 4;
constexpr int kNumSteps   = 16;
constexpr int kNumSlots   = 50;

// ---------------------------------------------------------------- styles
struct StyleInfo { const char* name; const char* category; const char* help; };

inline const std::array<StyleInfo, 32>& styles()
{
    static const std::array<StyleInfo, 32> s {{
        { "Warm Tube",     "Tube",    "Round, even-harmonic tube warmth." },
        { "Clean Tube",    "Tube",    "Very gentle tube glue. Barely there until pushed." },
        { "Hot Tube",      "Tube",    "Biased, pushed tube stage with more grit." },
        { "Triode",        "Tube",    "Asymmetric triode curve, thick and chewy." },
        { "Pentode",       "Tube",    "Symmetric power-tube compression, odd harmonics." },
        { "Warm Tape",     "Tape",    "Soft tape compression with a smooth top end." },
        { "Hot Tape",      "Tape",    "Slammed tape: squashed, dense and saturated." },
        { "Cassette",      "Tape",    "Lo-fi cassette: dark, soft and wobbly at the edges." },
        { "Iron",          "Transformer", "Transformer saturation that thickens the lows." },
        { "Heavy Iron",    "Transformer", "Overdriven transformer core, big low-end bloom." },
        { "Console",       "Transformer", "Subtle console channel colour." },
        { "Clean Amp",     "Amp",     "Edge-of-breakup guitar amp, sparkly and open." },
        { "Crunch Amp",    "Amp",     "Classic rock crunch with a mid push." },
        { "Lead Amp",      "Amp",     "High-gain cascaded lead tone." },
        { "Bass Amp",      "Amp",     "Keeps the low end clean while the mids growl." },
        { "Fuzz",          "Amp",     "Gated, asymmetric fuzz box." },
        { "Soft Clip",     "Clip",    "Smooth cubic clipper." },
        { "Hard Clip",     "Clip",    "Brick-wall clipping. Loud and direct." },
        { "Diode",         "Clip",    "Asymmetric diode clipper." },
        { "Rectify",       "Clip",    "Full-wave rectifier: adds an octave up." },
        { "Half Wave",     "Clip",    "Half-wave rectifier: buzzy and gated." },
        { "Exciter",       "Clip",    "Saturates only the highs for air and presence." },
        { "Sine Fold",     "Fold",    "Smooth sine wavefolding." },
        { "Foldback",      "Fold",    "Sharp triangle folding for metallic tones." },
        { "Harmonics",     "Fold",    "Adds 2nd and 3rd harmonics in a controlled way." },
        { "Bitcrush",      "Digital", "Reduces bit depth. Drive = fewer bits." },
        { "Downsample",    "Digital", "Sample-rate reduction. Drive = lower rate." },
        { "Ring",          "Digital", "Self ring-modulation. Drive sets the carrier." },
        { "Broken Speaker","Digital", "Torn cone rattle on loud peaks." },
        { "Octave Down",   "Digital", "Synthesises a sub-octave from the input." },
        { "Glue Comp",     "Dynamic", "Soft-knee compressor into a gentle saturator." },
        { "Smash",         "Dynamic", "Brutal limiting and clipping in one." }
    }};
    return s;
}

inline juce::StringArray styleNames()
{
    juce::StringArray a;
    for (auto& s : styles()) a.add (s.name);
    return a;
}

// ---------------------------------------------------------------- ids
inline juce::String bandId (int b, const char* p)  { return "b" + juce::String (b) + "_" + p; }
inline juce::String xoverId (int i)                { return "xover" + juce::String (i); }
inline juce::String slopeId (int i)                { return "slope" + juce::String (i); }
inline juce::String lfoId (int l, const char* p)   { return "lfo" + juce::String (l) + "_" + p; }
inline juce::String lfoStepId (int l, int s)       { return "lfo" + juce::String (l) + "_s" + juce::String (s); }
inline juce::String egId (int e, const char* p)    { return "eg" + juce::String (e) + "_" + p; }
inline juce::String efId (int e, const char* p)    { return "ef" + juce::String (e) + "_" + p; }
inline juce::String xyId (int x, const char* axis) { return "xy" + juce::String (x) + "_" + axis; }
inline juce::String macroId (int m)                { return "macro" + juce::String (m); }
inline juce::String slotId (int s, const char* p)  { return "mod" + juce::String (s) + "_" + p; }

// ---------------------------------------------------------------- modulation sources
enum class Src
{
    None = 0,
    Lfo1, Lfo2, Lfo3, Lfo4,
    Eg1, Eg2,
    Ef1, Ef2,
    Xy1X, Xy1Y, Xy2X, Xy2Y,
    Macro1, Macro2, Macro3, Macro4,
    Velocity, Note, ModWheel, PitchBend, Aftertouch,
    Count
};
constexpr int kNumSources = (int) Src::Count;

inline juce::StringArray sourceNames()
{
    static const char* raw[] = { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Envelope 1", "Envelope 2",
             "Follower 1", "Follower 2", "XY 1 · X", "XY 1 · Y", "XY 2 · X", "XY 2 · Y",
             "Macro 1", "Macro 2", "Macro 3", "Macro 4",
             "MIDI Velocity", "MIDI Note", "MIDI Mod Wheel", "MIDI Pitch Bend", "MIDI Aftertouch" };
    juce::StringArray a;
    for (auto* r : raw) a.add (juce::String::fromUTF8 (r));
    return a;
}

// ---------------------------------------------------------------- modulation targets
constexpr int kBandTargets = 7; // drive, fb, fbfreq, dyn, tone, mix, level
inline const char* bandTargetParam (int t)
{
    static const char* p[] = { "drive", "fb", "fbf", "dyn", "tone", "mix", "lvl" };
    return p[t];
}
inline const char* bandTargetLabel (int t)
{
    static const char* p[] = { "Drive", "Feedback", "FB Freq", "Dynamics", "Tone", "Mix", "Level" };
    return p[t];
}
constexpr int kNumTargets = 1 + kMaxBands * kBandTargets + kMaxXovers + 3; // none + bands + xovers + mix, out, input

inline juce::String targetParamId (int t)
{
    if (t <= 0) return {};
    --t;
    if (t < kMaxBands * kBandTargets) return bandId (t / kBandTargets, bandTargetParam (t % kBandTargets));
    t -= kMaxBands * kBandTargets;
    if (t < kMaxXovers) return xoverId (t);
    t -= kMaxXovers;
    if (t == 0) return "mix";
    if (t == 1) return "out";
    return "in";
}

inline juce::String targetName (int t)
{
    if (t <= 0) return "None";
    int u = t - 1;
    if (u < kMaxBands * kBandTargets) return "Band " + juce::String (u / kBandTargets + 1) + juce::String::fromUTF8 (" \xc2\xb7 ") + bandTargetLabel (u % kBandTargets);
    u -= kMaxBands * kBandTargets;
    if (u < kMaxXovers) return "Crossover " + juce::String (u + 1);
    u -= kMaxXovers;
    if (u == 0) return "Global Mix";
    if (u == 1) return "Output";
    return "Input";
}

inline juce::StringArray targetNames()
{
    juce::StringArray a;
    for (int t = 0; t < kNumTargets; ++t) a.add (targetName (t));
    return a;
}

inline int targetForParamId (const juce::String& id)
{
    for (int t = 1; t < kNumTargets; ++t)
        if (targetParamId (t) == id) return t;
    return 0;
}

inline juce::StringArray syncDivNames()
{
    return { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/2 T", "1/4.", "1/4", "1/4 T",
             "1/8.", "1/8", "1/8 T", "1/16.", "1/16", "1/16 T", "1/32" };
}
inline double syncDivBeats (int i)
{
    static const double b[] = { 32, 16, 8, 4, 2, 4.0 / 3.0, 1.5, 1, 2.0 / 3.0,
                                0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
    return b[juce::jlimit (0, 15, i)];
}

inline juce::StringArray lfoShapeNames()
{
    return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random", "Smooth Random", "Steps" };
}

// ---------------------------------------------------------------- layout
inline juce::NormalisableRange<float> freqRange()
{
    juce::NormalisableRange<float> r (20.0f, 20000.0f);
    r.setSkewForCentre (632.0f);
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    auto pct   = [] (float v, int) { return String (juce::roundToInt (v)) + "%"; };
    auto dbStr = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };
    auto hz    = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (juce::roundToInt (v)) + " Hz"; };
    auto bip   = [] (float v, int) { return (v > 0 ? "+" : "") + String (juce::roundToInt (v)) + "%"; };

    auto F = [&] (const String& id, const String& name, NormalisableRange<float> r, float def,
                  std::function<String (float, int)> fmt = nullptr, const String& label = {})
    {
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withLabel (label)));
    };
    auto C = [&] (const String& id, const String& name, StringArray choices, int def)
    { L.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def)); };
    auto B = [&] (const String& id, const String& name, bool def)
    { L.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto I = [&] (const String& id, const String& name, int lo, int hi, int def)
    { L.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    // global
    I ("bands", "Bands", 1, kMaxBands, 1);
    C ("phase", "Phase Mode", { "Zero Latency", "Linear Phase" }, 0);
    C ("hq", "Quality", { "Eco", "2x", "4x", "8x", "16x", "32x" }, 1);
    F ("in", "Input", { -24.0f, 24.0f, 0.01f }, 0.0f, dbStr);
    F ("out", "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, dbStr);
    F ("mix", "Mix", { 0.0f, 100.0f, 0.01f }, 100.0f, pct);
    B ("autogain", "Auto Gain", true);

    const float defX[] = { 150.0f, 900.0f, 3000.0f, 7000.0f, 12000.0f };
    for (int i = 0; i < kMaxXovers; ++i)
    {
        F (xoverId (i), "Crossover " + String (i + 1), freqRange(), defX[i], hz);
        C (slopeId (i), "Slope " + String (i + 1), { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 2);
    }

    for (int b = 0; b < kMaxBands; ++b)
    {
        const String n = "B" + String (b + 1) + " ";
        C (bandId (b, "style"), n + "Style", styleNames(), 0);
        F (bandId (b, "drive"), n + "Drive", { 0.0f, 100.0f, 0.01f }, 25.0f, pct);
        F (bandId (b, "fb"),    n + "Feedback", { 0.0f, 100.0f, 0.01f }, 0.0f, pct);
        F (bandId (b, "fbf"),   n + "Feedback Freq", freqRange(), 1000.0f, hz);
        F (bandId (b, "dyn"),   n + "Dynamics", { -100.0f, 100.0f, 0.01f }, 0.0f, bip);
        F (bandId (b, "tone"),  n + "Tone", { -100.0f, 100.0f, 0.01f }, 0.0f, bip);
        F (bandId (b, "mix"),   n + "Mix", { 0.0f, 100.0f, 0.01f }, 100.0f, pct);
        F (bandId (b, "lvl"),   n + "Level", { -36.0f, 18.0f, 0.01f }, 0.0f, dbStr);
        F (bandId (b, "width"), n + "Width", { 0.0f, 200.0f, 0.01f }, 100.0f, pct);
        C (bandId (b, "ch"),    n + "Channels", { "Stereo", "Mid", "Side", "Left", "Right" }, 0);
        B (bandId (b, "solo"),  n + "Solo", false);
        B (bandId (b, "mute"),  n + "Mute", false);
        B (bandId (b, "byp"),   n + "Bypass", false);
    }

    for (int l = 0; l < kNumLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        NormalisableRange<float> rate (0.01f, 40.0f); rate.setSkewForCentre (1.0f);
        F (lfoId (l, "rate"), n + "Rate", rate, 1.0f, [] (float v, int) { return String (v, v < 1 ? 2 : 1) + " Hz"; });
        B (lfoId (l, "sync"), n + "Sync", false);
        C (lfoId (l, "div"), n + "Division", syncDivNames(), 7);
        C (lfoId (l, "shape"), n + "Shape", lfoShapeNames(), 0);
        I (lfoId (l, "nsteps"), n + "Steps", 2, kNumSteps, 8);
        F (lfoId (l, "glide"), n + "Glide", { 0.0f, 100.0f, 0.01f }, 0.0f, pct);
        F (lfoId (l, "phase"), n + "Phase", { 0.0f, 360.0f, 0.1f }, 0.0f, [] (float v, int) { return String (roundToInt (v)) + "°"; });
        B (lfoId (l, "retrig"), n + "Retrigger", false);
        for (int s = 0; s < kNumSteps; ++s)
        {
            const float v = (float) std::sin ((s + 0.5) / kNumSteps * MathConstants<double>::twoPi);
            F (lfoStepId (l, s), n + "Step " + String (s + 1), { -1.0f, 1.0f, 0.001f }, v);
        }
    }

    for (int e = 0; e < kNumEgs; ++e)
    {
        const String n = "Env " + String (e + 1) + " ";
        auto ms = [] (float v, int) { return v >= 1000 ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; };
        NormalisableRange<float> t (0.5f, 5000.0f); t.setSkewForCentre (150.0f);
        F (egId (e, "a"), n + "Attack", t, 10.0f, ms);
        F (egId (e, "d"), n + "Decay", t, 250.0f, ms);
        F (egId (e, "s"), n + "Sustain", { 0.0f, 100.0f, 0.01f }, 50.0f, pct);
        F (egId (e, "r"), n + "Release", t, 300.0f, ms);
        F (egId (e, "curve"), n + "Curve", { -100.0f, 100.0f, 0.01f }, 0.0f, bip);
        C (egId (e, "trig"), n + "Trigger", { "MIDI", "Audio" }, 1);
        F (egId (e, "thr"), n + "Threshold", { -60.0f, 0.0f, 0.1f }, -24.0f, dbStr);
    }

    for (int e = 0; e < kNumEfs; ++e)
    {
        const String n = "Follower " + String (e + 1) + " ";
        auto ms = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
        NormalisableRange<float> t (0.1f, 2000.0f); t.setSkewForCentre (60.0f);
        F (efId (e, "att"), n + "Attack", t, 5.0f, ms);
        F (efId (e, "rel"), n + "Release", t, 150.0f, ms);
        F (efId (e, "gain"), n + "Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, dbStr);
        B (efId (e, "trans"), n + "Transient", false);
    }

    for (int x = 0; x < kNumXy; ++x)
    {
        F (xyId (x, "x"), "XY " + String (x + 1) + " X", { 0.0f, 1.0f, 0.0001f }, 0.5f);
        F (xyId (x, "y"), "XY " + String (x + 1) + " Y", { 0.0f, 1.0f, 0.0001f }, 0.5f);
    }
    for (int m = 0; m < kNumMacros; ++m)
        F (macroId (m), "Macro " + String (m + 1), { 0.0f, 1.0f, 0.0001f }, 0.0f);

    for (int s = 0; s < kNumSlots; ++s)
    {
        const String n = "Mod " + String (s + 1) + " ";
        C (slotId (s, "src"), n + "Source", sourceNames(), 0);
        C (slotId (s, "dst"), n + "Target", targetNames(), 0);
        F (slotId (s, "amt"), n + "Amount", { -100.0f, 100.0f, 0.01f }, 50.0f, bip);
    }
    return L;
}
} // namespace anjis
