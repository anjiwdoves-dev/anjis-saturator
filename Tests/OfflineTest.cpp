// Offline DSP checks (Linux dev only, not part of the plugin).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
using namespace anjis;

static void setP (AnjisAudioProcessor& p, const juce::String& id, float v) { p.set (id, v); }

static double runSine (AnjisAudioProcessor& p, double f, double sr, int block)
{
    const int total = (int) sr; // 1 s
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    double ph = 0, sumIn = 0, sumOut = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i) { float s = 0.25f * (float) std::sin (ph); ph += 2 * kPi * f / sr; buf.setSample (0, i, s); buf.setSample (1, i, s); }
        p.processBlock (buf, midi);
        if (pos > total / 2)
            for (int i = 0; i < block; ++i) { sumIn += 0.0625 / 2; sumOut += buf.getSample (0, i) * buf.getSample (0, i); }
    }
    return 10 * std::log10 (sumOut / sumIn);
}

static int impulsePeak (AnjisAudioProcessor& p, int block, float& peakVal, bool& finite)
{
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    int best = -1; peakVal = 0; finite = true;
    for (int pos = 0; pos < 16384; pos += block)
    {
        buf.clear();
        if (pos == 0) { buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f); }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i)
        {
            float v = std::abs (buf.getSample (0, i));
            if (! std::isfinite (v)) finite = false;
            if (v > peakVal) { peakVal = v; best = pos + i; }
        }
    }
    return best;
}

static void snapshot (const char* path, int preset, int bands, int modTab, bool picker)
{
    AnjisAudioProcessor p;
    const double sr = 48000; const int block = 480;
    p.prepareToPlay (sr, block);
    PresetManager pm (p);
    if (preset >= 0) pm.loadIndex (preset);
    if (bands > 0)
    {
        p.set ("bands", (float) bands);
        p.set (bandId (1, "style"), 12); p.set (bandId (1, "drive"), 60);
        p.set (bandId (2, "style"), 21); p.set (bandId (3, "style"), 25);
        p.set (slotId (0, "src"), 1); p.set (slotId (0, "dst"), 1 + kBandTargets); p.set (slotId (0, "amt"), 35);
        p.set (slotId (1, "src"), 5); p.set (slotId (1, "dst"), 1 + kBandTargets * 6); p.set (slotId (1, "amt"), -30);
        p.selectedBand = 1;
    }
    std::unique_ptr<AnjisEditor> ed (dynamic_cast<AnjisEditor*> (p.createEditor()));
    ed->setSize (1100, 800);
    if (modTab >= 0) ed->getModPanel().select (modTab);
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    juce::Random r (1); double ph = 0;
    for (int f = 0; f < 90; ++f)
    {
        for (int k = 0; k < 2; ++k)
        {
            for (int i = 0; i < block; ++i)
            {
                float drum = (f % 12 < 2) ? (r.nextFloat() * 2 - 1) * 0.5f : 0.0f;
                float s = 0.3f * (float) std::sin (ph) + 0.12f * (float) std::sin (ph * 5.03) + drum + (r.nextFloat() - 0.5f) * 0.02f;
                ph += 2 * kPi * 110 / sr;
                buf.setSample (0, i, s); buf.setSample (1, i, s * 0.9f);
            }
            p.processBlock (buf, midi);
        }
        ed->testTick();
    }
    if (picker) { ed->testOpenPicker(); for (int f = 0; f < 30; ++f) ed->testTick(); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File (path).deleteFile();
    juce::File outFile (path); juce::FileOutputStream os (outFile);
    juce::PNGImageFormat().writeImageToStream (img, os);
    printf ("wrote %s\n", path);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "snap")
    {
        snapshot ("/tmp/snap_init.png", 0, 0, 0, false);
        snapshot ("/tmp/snap_multi.png", -1, 4, 0, false);
        snapshot ("/tmp/snap_steps.png", 17, 0, 0, false);
        snapshot ("/tmp/snap_env.png", -1, 4, 4, false);
        snapshot ("/tmp/snap_routing.png", -1, 4, 11, false);
        snapshot ("/tmp/snap_picker.png", -1, 4, 0, true);
        snapshot ("/tmp/snap_xy.png", -1, 2, 8, false);
        return 0;
    }
    const double sr = 48000; const int block = 480;
    int failures = 0;

    // 1) crossover flatness: band mix 0 => bands sum back to input magnitude
    for (int phase = 0; phase < 2; ++phase)
     for (int slope = 0; slope < 4; ++slope)
      for (int nb : { 2, 4, 6 })
      {
        AnjisAudioProcessor p;
        setP (p, "bands", (float) nb); setP (p, "phase", (float) phase); setP (p, "hq", 1);
        setP (p, "autogain", 0);
        for (int i = 0; i < 5; ++i) setP (p, slopeId (i), (float) slope);
        for (int b = 0; b < 6; ++b) setP (p, bandId (b, "mix"), 0);
        p.prepareToPlay (sr, block);
        if (phase == 1) { juce::Thread::sleep (400); }
        double worst = 0;
        for (double f : { 60.0, 140.0, 900.0, 3000.0, 7000.0, 12000.0 })
        {
            if (phase == 1) { p.prepareToPlay (sr, block); }
            worst = std::max (worst, std::abs (runSine (p, f, sr, block)));
        }
        const bool ok = worst < 0.25;
        failures += ! ok;
        printf ("flat  phase=%d slope=%d bands=%d  worst dev %.3f dB %s\n", phase, slope, nb, worst, ok ? "OK" : "FAIL");
      }

    // 2) latency matches impulse position (wet path, band mix 0)
    for (int phase = 0; phase < 2; ++phase)
     for (int hq = 0; hq < 6; ++hq)
     {
        AnjisAudioProcessor p;
        setP (p, "phase", (float) phase); setP (p, "hq", (float) hq); setP (p, bandId (0, "mix"), 0);
        p.prepareToPlay (sr, block);
        juce::Thread::sleep (100);
        float pk; bool fin;
        int pos = impulsePeak (p, block, pk, fin);
        int lat = p.getLatencySamples();
        bool ok = std::abs (pos - lat) <= 2;
        failures += ! ok;
        printf ("latency phase=%d hq=%d reported=%d measured=%d %s\n", phase, hq, lat, pos, ok ? "OK" : "FAIL");
     }

    // 3) every style at max drive + feedback stays finite and bounded
    for (int s = 0; s < 32; ++s)
    {
        AnjisAudioProcessor p;
        setP (p, "bands", 3); setP (p, "hq", 2);
        for (int b = 0; b < 3; ++b) { setP (p, bandId (b, "style"), (float) s); setP (p, bandId (b, "drive"), 100); setP (p, bandId (b, "fb"), 100); setP (p, bandId (b, "dyn"), 100); setP (p, bandId (b, "tone"), 100); }
        setP (p, slotId (0, "src"), 1); setP (p, slotId (0, "dst"), 1); setP (p, slotId (0, "amt"), 100);
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
        juce::Random r (s);
        float pk = 0; bool fin = true;
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int k = 0; k < 200; ++k)
        {
            for (int i = 0; i < block; ++i) { float v = (r.nextFloat() * 2 - 1) * 0.9f; buf.setSample (0, i, v); buf.setSample (1, i, v * 0.7f); }
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i) { float v = buf.getSample (0, i); if (! std::isfinite (v)) fin = false; pk = std::max (pk, std::abs (v)); }
        }
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        bool ok = fin && pk < 8.0f;
        failures += ! ok;
        printf ("style %2d %-15s peak %.2f  cpu %.0f%% %s\n", s, styles()[(size_t) s].name, pk, ms / 2000.0 * 100, ok ? "OK" : "FAIL");
    }
    // 4) band insert/remove and state round trip
    {
        AnjisAudioProcessor p; p.prepareToPlay (sr, block);
        p.set (bandId (0, "style"), 5);
        p.insertBandAt (1000);           // 2 bands, xover0 = 1000
        p.set (bandId (1, "style"), 13);
        p.insertBandAt (200);            // 3 bands: xover 200, 1000 ; band0 tape, band1 tape(copy), band2 lead
        bool ok = (int) p.get ("bands") == 3 && std::abs (p.get (xoverId (0)) - 200) < 1 && std::abs (p.get (xoverId (1)) - 1000) < 2
                  && (int) p.get (bandId (2, "style")) == 13 && (int) p.get (bandId (1, "style")) == 5;
        p.set (slotId (0, "src"), 1); p.set (slotId (0, "dst"), 1 + 2 * kBandTargets); // LFO1 -> band3 drive
        p.removeBand (1);
        ok = ok && (int) p.get ("bands") == 2 && (int) p.get (bandId (1, "style")) == 13 && (int) p.get (slotId (0, "dst")) == 1 + kBandTargets;
        juce::MemoryBlock mb; p.getStateInformation (mb);
        AnjisAudioProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        ok = ok && (int) q.get ("bands") == 2 && (int) q.get (bandId (1, "style")) == 13;
        failures += ! ok;
        printf ("band insert/remove/state %s\n", ok ? "OK" : "FAIL");
    }

    // 5) worst-case CPU: 6 bands, linear phase, 32x, all mod slots busy
    for (int hq : { 1, 3, 5 })
    {
        AnjisAudioProcessor p;
        p.set ("bands", 6); p.set ("phase", 1); p.set ("hq", (float) hq);
        for (int s = 0; s < kNumSlots; ++s) { p.set (slotId (s, "src"), (float) (1 + s % 21)); p.set (slotId (s, "dst"), (float) (1 + s % 47)); }
        p.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; juce::Random r;
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        const int blocks = (int) (sr * 3 / 512);
        for (int k = 0; k < blocks; ++k)
        {
            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, r.nextFloat() - 0.5f); buf.setSample (1, i, r.nextFloat() - 0.5f); }
            p.processBlock (buf, midi);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        printf ("cpu 6 bands linear hq=%d: %.1f%% of one core\n", hq, ms / 3000.0 * 100.0);
    }

    // 6) every factory preset loads and processes cleanly
    {
        AnjisAudioProcessor p; p.prepareToPlay (sr, block);
        PresetManager pm (p);
        auto list = pm.list();
        bool ok = true;
        for (int i = 0; i < list.size(); ++i)
        {
            pm.loadIndex (i);
            juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi; juce::Random r (i);
            for (int k = 0; k < 100; ++k)
            {
                for (int j = 0; j < block; ++j) { float v = (r.nextFloat() - 0.5f) * 0.6f; buf.setSample (0, j, v); buf.setSample (1, j, v); }
                p.processBlock (buf, midi);
                for (int j = 0; j < block; ++j) if (! std::isfinite (buf.getSample (0, j)) || std::abs (buf.getSample (0, j)) > 6) ok = false;
            }
        }
        failures += ! ok;
        printf ("%d factory presets %s\n", list.size(), ok ? "OK" : "FAIL");
    }

    printf ("\n%d failures\n", failures);
    return failures;
}
