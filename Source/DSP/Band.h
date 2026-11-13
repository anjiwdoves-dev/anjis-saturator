#pragma once
#include <JuceHeader.h>
#include "Shaper.h"

namespace anjis
{
struct BandParams
{
    float drive = 25, fb = 0, fbf = 1000, dyn = 0, tone = 0, mix = 100, lvl = 0, width = 100;
    int style = 0, ch = 0;
    bool bypass = false;
};

// One band's saturation chain, running at the oversampled rate.
class BandProcessor
{
public:
    static constexpr int kMaxStages = 5;

    void prepare (double sampleRate, int maxChunk)
    {
        sr = sampleRate;
        for (int s = 0; s < kMaxStages; ++s)
        {
            os[s] = std::make_unique<juce::dsp::Oversampling<float>> (
                2, (size_t) (s + 1), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            os[s]->initProcessing ((size_t) maxChunk);
        }
        osSr = 0;
        reset();
    }

    float getLatency (int stages) const
    {
        return stages > 0 ? os[stages - 1]->getLatencyInSamples() : 0.0f;
    }

    void reset()
    {
        for (auto& o : os) if (o) o->reset();
        for (int c = 0; c < 2; ++c)
        {
            shaper[c].reset(); fbSvf[c].reset(); tilt[c].reset(); dc[c].reset(); env[c].reset();
            fbState[c] = 0;
        }
        first = true;
        agIn = agOut = 1.0e-4f; agGain = 1.0f;
    }

    // Processes one chunk (<= maxChunk samples, 2 channels) in place.
    void process (juce::dsp::AudioBlock<float>& block, const BandParams& p, int stages,
                  float centreHz, bool autoGain)
    {
        const int n = (int) block.getNumSamples();
        if (stages != lastStages)
        {
            if (stages > 0) os[stages - 1]->reset();
            lastStages = stages; osSr = 0;
        }

        if (first) { cur = p; first = false; }

        // stereo routing
        auto* L = block.getChannelPointer (0);
        auto* R = block.getChannelPointer (1);
        const bool ms = (p.ch == 1 || p.ch == 2);
        if (ms) toMs (L, R, n);

        // input level meter (pre drive)
        float pk = 0;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        inPeak = pk;

        juce::dsp::AudioBlock<float> up = stages > 0 ? os[stages - 1]->processSamplesUp (block) : block;
        const double rate = sr * (double) (1 << stages);
        if (rate != osSr)
        {
            osSr = rate;
            for (int c = 0; c < 2; ++c)
            {
                shaper[c].setSampleRate (rate);
                dc[c].setCutoff (8.0, rate);
                env[c].set (2.0, 60.0, rate);
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            fbSvf[c].set (p.fbf, rate, 0.45f);
            tilt[c].setCutoff (centreHz, rate);
        }

        const int m = (int) up.getNumSamples();
        const float inv = 1.0f / (float) m;

        auto driveGain = [] (const BandParams& q) { return dbToGain (q.drive * 0.4f); };
        const float g0 = driveGain (cur), g1 = driveGain (p);
        const float c0 = 1.0f / std::sqrt (g0), c1 = 1.0f / std::sqrt (g1);
        const float tl0 = dbToGain (-cur.tone * 0.09f), tl1 = dbToGain (-p.tone * 0.09f);
        const float th0 = dbToGain (cur.tone * 0.09f),  th1 = dbToGain (p.tone * 0.09f);
        const float lv0 = dbToGain (cur.lvl), lv1 = dbToGain (p.lvl);

        float sumIn = 0, sumOut = 0;
        const float ag0 = agGain;

        for (int c = 0; c < 2; ++c)
        {
            const bool active = ! p.bypass && (p.ch == 0 || ((p.ch == 1 || p.ch == 3) && c == 0) || ((p.ch == 2 || p.ch == 4) && c == 1));
            auto* x = up.getChannelPointer ((size_t) c);
            if (! active) continue;

            auto& sh = shaper[c];
            for (int i = 0; i < m; ++i)
            {
                const float t = (float) i * inv;
                const float amt  = (cur.drive + (p.drive - cur.drive) * t) * 0.01f;
                const float gain = g0 + (g1 - g0) * t;
                const float comp = c0 + (c1 - c0) * t;
                const float fbA  = (cur.fb + (p.fb - cur.fb) * t) * 0.0095f;
                const float dyn  = (cur.dyn + (p.dyn - cur.dyn) * t) * 0.01f;
                const float mix  = (cur.mix + (p.mix - cur.mix) * t) * 0.01f;
                const float gl   = tl0 + (tl1 - tl0) * t, gh = th0 + (th1 - th0) * t;

                const float raw = x[i];
                const float in  = raw + fbA * fbState[c];
                float y = sh.process (p.style, in, amt, gain, comp);

                fbState[c] = ftanh (fbSvf[c].bp (y) * 1.5f);

                if (dyn != 0.0f)
                {
                    const float e = env[c].process (raw) + 1.0e-5f;
                    const float d = std::exp (dyn * 0.6f * std::log (e * 5.0f));
                    y *= std::clamp (d, 0.1f, 4.0f);
                }

                const float lo = tilt[c].lp (y);
                y = lo * gl + (y - lo) * gh;
                y = dc[c].hp (y);
                y = 2.0f * ftanh (y * 0.5f); // safety ceiling, transparent at normal levels

                const float out = raw + (y - raw) * mix;
                sumIn += raw * raw; sumOut += out * out;
                x[i] = out * (ag0 + (agGain - ag0) * t);
            }
        }

        // auto gain: slow loudness match between band input and output
        const float k = 1.0f - std::exp (-(float) m / (float) (rate * 0.35));
        agIn  += (sumIn  - agIn)  * k;
        agOut += (sumOut - agOut) * k;
        float target = autoGain ? std::sqrt ((agIn + 1.0e-9f) / (agOut + 1.0e-9f)) : 1.0f;
        if (agIn < 1.0e-7f) target = agGain; // hold during silence
        agGain = std::clamp (agGain + (std::clamp (target, 0.05f, 3.0f) - agGain) * 0.15f, 0.05f, 3.0f);

        if (stages > 0) os[stages - 1]->processSamplesDown (block);

        if (ms) fromMs (L, R, n);

        // width + level (base rate)
        const float w0 = cur.width * 0.01f, w1 = p.width * 0.01f;
        float pkOut = 0, rms = 0;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) n;
            const float w = w0 + (w1 - w0) * t;
            const float lv = lv0 + (lv1 - lv0) * t;
            const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]) * w;
            L[i] = (mid + side) * lv; R[i] = (mid - side) * lv;
            pkOut = std::max (pkOut, std::abs (L[i]));
            rms += L[i] * L[i] + R[i] * R[i];
        }
        outPeak = pkOut;
        outRms = std::sqrt (rms / (2.0f * (float) n));
        cur = p;
    }

    float inPeak = 0, outPeak = 0, outRms = 0, agGain = 1.0f;

private:
    static void toMs (float* L, float* R, int n)
    { for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]); L[i] = m; R[i] = s; } }
    static void fromMs (float* L, float* R, int n)
    { for (int i = 0; i < n; ++i) { const float m = L[i], s = R[i]; L[i] = m + s; R[i] = m - s; } }

    double sr = 44100, osSr = 0;
    int lastStages = -1;
    bool first = true;
    BandParams cur;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kMaxStages> os;
    Shaper shaper[2];
    Svf fbSvf[2];
    OnePole tilt[2], dc[2];
    EnvFollower env[2];
    float fbState[2] {};
    float agIn = 1.0e-4f, agOut = 1.0e-4f;
};
} // namespace anjis
