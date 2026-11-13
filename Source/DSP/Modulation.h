#pragma once
#include <JuceHeader.h>
#include "../Params.h"
#include "Filters.h"

namespace anjis
{
struct LfoSettings  { float rate = 1; bool sync = false; int div = 7, shape = 0, nSteps = 8; float glide = 0, phase = 0; bool retrig = false; float steps[kNumSteps] {}; };
struct EgSettings   { float a = 10, d = 250, s = 50, r = 300, curve = 0; int trig = 1; float thr = -24; };
struct EfSettings   { float att = 5, rel = 150, gain = 0; bool trans = false; };

struct Transport { bool playing = false; bool hasPpq = false; double ppq = 0, bpm = 120; };

// Control-rate modulation sources.
class ModEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : lfo) l = {};
        for (auto& e : eg) e = {};
        for (auto& f : ef) { f.fast.reset(); f.slow.reset(); }
        trigEnv.set (1.0, 40.0, sr);
    }

    // MIDI
    void noteOn (int note, float vel)
    {
        heldNotes = std::min (heldNotes + 1, 128);
        lastNote = note; velocity = vel;
        for (auto& e : eg) e.midiGate = true, e.retrigger = true;
        for (auto& l : lfo) l.noteRetrig = true;
    }
    void noteOff (int)
    {
        heldNotes = std::max (0, heldNotes - 1);
        if (heldNotes == 0) for (auto& e : eg) e.midiGate = false;
    }
    void allNotesOff() { heldNotes = 0; for (auto& e : eg) e.midiGate = false; }
    void controller (int cc, int value) { if (cc == 1) modWheel = (float) value / 127.0f; }
    void pitchWheel (int value) { pitchBend = juce::jlimit (-1.0f, 1.0f, ((float) value - 8192.0f) / 8191.0f); }
    void pressure (int value) { aftertouch = (float) value / 127.0f; }

    // Advances all sources by one chunk. 'mono' is the chunk's input (mono sum).
    void tick (const float* mono, int n, const LfoSettings* ls, const EgSettings* es, const EfSettings* fs,
               const float* xy, const float* macros, const Transport& tp)
    {
        const double dt = (double) n / sr;

        // followers + audio trigger detection
        float pk = 0;
        for (int i = 0; i < n; ++i) pk = std::max (pk, trigEnv.process (mono[i]));
        const float levelDb = gainToDb (pk);

        for (int e = 0; e < kNumEfs; ++e)
        {
            auto& f = ef[e];
            f.fast.set (fs[e].att, fs[e].rel, sr);
            f.slow.set (fs[e].att * 8.0, fs[e].rel * 3.0, sr);
            float vf = 0, vs = 0;
            for (int i = 0; i < n; ++i) { vf = f.fast.process (mono[i]); vs = f.slow.process (mono[i]); }
            float v = fs[e].trans ? std::max (0.0f, vf - vs) * 4.0f : vf;
            v *= dbToGain (fs[e].gain);
            values[(int) Src::Ef1 + e] = std::clamp ((gainToDb (v) + 54.0f) / 54.0f, 0.0f, 1.0f);
        }

        for (int l = 0; l < kNumLfos; ++l)
            values[(int) Src::Lfo1 + l] = tickLfo (lfo[l], ls[l], dt, tp);

        for (int e = 0; e < kNumEgs; ++e)
            values[(int) Src::Eg1 + e] = tickEg (eg[e], es[e], dt, levelDb);

        values[(int) Src::Xy1X] = xy[0]; values[(int) Src::Xy1Y] = xy[1];
        values[(int) Src::Xy2X] = xy[2]; values[(int) Src::Xy2Y] = xy[3];
        for (int m = 0; m < kNumMacros; ++m) values[(int) Src::Macro1 + m] = macros[m];
        values[(int) Src::Velocity]   = velocity;
        values[(int) Src::Note]       = (float) lastNote / 127.0f;
        values[(int) Src::ModWheel]   = modWheel;
        values[(int) Src::PitchBend]  = pitchBend;
        values[(int) Src::Aftertouch] = aftertouch;
        values[0] = 0;
    }

    float values[kNumSources] {};
    float lfoPhase[kNumLfos] {};
    float egLevel[kNumEgs] {};

private:
    struct LfoState { double phase = 0; float rnd = 0, rndPrev = 0, rndNext = 0; bool noteRetrig = false; };
    struct EgState  { int stage = 0; float level = 0, from = 0; double t = 0; bool midiGate = false, audioGate = false, retrigger = false, gate = false; };
    struct EfState  { EnvFollower fast, slow; };

    float rand01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xffffff) / 16777216.0f; }

    float tickLfo (LfoState& st, const LfoSettings& s, double dt, const Transport& tp)
    {
        const int idx = (int) (&st - lfo);
        double ph;
        const double beats = syncDivBeats (s.div);
        const double prev = st.phase;
        if (s.sync && tp.playing && tp.hasPpq)
        {
            ph = std::fmod (tp.ppq / beats, 1.0);
            if (ph < 0) ph += 1.0;
        }
        else
        {
            const double hz = s.sync ? (tp.bpm / 60.0) / beats : (double) s.rate;
            if (s.retrig && st.noteRetrig) st.phase = 0;
            ph = st.phase + hz * dt;
            ph -= std::floor (ph);
        }
        st.noteRetrig = false;
        const bool wrapped = ph < prev;
        st.phase = ph;
        double p = ph + s.phase / 360.0; p -= std::floor (p);
        lfoPhase[idx] = (float) p;

        if (wrapped)
        {
            st.rndPrev = st.rndNext;
            st.rndNext = rand01() * 2.0f - 1.0f;
            st.rnd = st.rndNext;
        }
        const float pf = (float) p;
        switch (s.shape)
        {
            case 0: return std::sin (pf * 6.2831853f);
            case 1: return pf < 0.25f ? pf * 4.0f : pf < 0.75f ? 2.0f - pf * 4.0f : pf * 4.0f - 4.0f;
            case 2: return pf * 2.0f - 1.0f;
            case 3: return 1.0f - pf * 2.0f;
            case 4: return pf < 0.5f ? 1.0f : -1.0f;
            case 5: return st.rnd;
            case 6: { const float c = 0.5f - 0.5f * std::cos ((float) ph * 3.14159265f); return st.rndPrev + (st.rndNext - st.rndPrev) * c; }
            default:
            {
                const int n = std::clamp (s.nSteps, 2, kNumSteps);
                const float pos = pf * (float) n;
                const int i = std::min ((int) pos, n - 1);
                const float frac = pos - (float) i;
                const float cur = s.steps[i], before = s.steps[(i + n - 1) % n];
                const float g = s.glide * 0.01f;
                if (g <= 0.001f || frac >= g) return cur;
                float t = frac / g; t = t * t * (3.0f - 2.0f * t);
                return before + (cur - before) * t;
            }
        }
    }

    float tickEg (EgState& st, const EgSettings& s, double dt, float levelDb)
    {
        bool gate;
        if (s.trig == 0) gate = st.midiGate;
        else
        {
            if (! st.audioGate && levelDb > s.thr) { st.audioGate = true; st.retrigger = true; }
            else if (st.audioGate && levelDb < s.thr - 6.0f) st.audioGate = false;
            gate = st.audioGate;
        }
        if (gate && (st.retrigger || ! st.gate)) { st.stage = 1; st.t = 0; st.from = st.level; }
        if (! gate && st.gate) { st.stage = 4; st.t = 0; st.from = st.level; }
        st.retrigger = false;
        st.gate = gate;

        st.t += dt * 1000.0;
        const float sus = s.s * 0.01f;
        switch (st.stage)
        {
            case 1: { const float k = (float) std::min (1.0, st.t / s.a); st.level = st.from + (1.0f - st.from) * k; if (k >= 1) { st.stage = 2; st.t = 0; } break; }
            case 2: { const float k = (float) std::min (1.0, st.t / s.d); st.level = 1.0f + (sus - 1.0f) * k; if (k >= 1) st.stage = 3; break; }
            case 3: st.level = sus; break;
            case 4: { const float k = (float) std::min (1.0, st.t / s.r); st.level = st.from * (1.0f - k); if (k >= 1) st.stage = 0; break; }
            default: st.level = 0; break;
        }
        const float c = s.curve * 0.01f;
        const float out = std::pow (std::clamp (st.level, 0.0f, 1.0f), std::exp (c * 1.6f));
        egLevel[(int) (&st - eg)] = out;
        return out;
    }

    double sr = 44100;
    LfoState lfo[kNumLfos];
    EgState eg[kNumEgs];
    EfState ef[kNumEfs];
    EnvFollower trigEnv;
    int heldNotes = 0, lastNote = 60;
    float velocity = 0, modWheel = 0, pitchBend = 0, aftertouch = 0;
    uint32_t rng = 0x12345678u;
};

inline bool isBipolar (int src)
{
    return (src >= (int) Src::Lfo1 && src <= (int) Src::Lfo4) || src == (int) Src::PitchBend;
}
} // namespace anjis
