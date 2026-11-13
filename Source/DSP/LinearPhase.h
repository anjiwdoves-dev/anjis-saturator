#pragma once
#include <JuceHeader.h>
#include "Filters.h"

namespace anjis
{
// Linear-phase band splitter. Band kernels are complementary zero-phase
// magnitude responses, so all bands sum back to a pure delay.
// Kernels are designed on a worker thread and handed to the audio thread
// without locking or allocating there.
class LinearPhaseSplitter : private juce::Thread
{
public:
    static constexpr int kMaxBands = 6;

    LinearPhaseSplitter() : juce::Thread ("Anjis LP kernels") {}
    ~LinearPhaseSplitter() override { stopThread (2000); }

    struct Design
    {
        int nBands = 1;
        float f[kMaxBands] {};
        int s[kMaxBands] {};
        bool operator!= (const Design& o) const
        {
            if (nBands != o.nBands) return true;
            for (int i = 0; i < nBands - 1; ++i)
                if (std::abs (f[i] - o.f[i]) > f[i] * 0.004f || s[i] != o.s[i]) return true;
            return false;
        }
    };

    void prepare (double sampleRate, int maxChunk, const Design& d)
    {
        stopThread (2000);
        sr = sampleRate;
        N = 4096;
        while (N < 4096 * sampleRate / 48000.0) N *= 2;
        cosTable.resize ((size_t) N);
        for (int i = 0; i < N; ++i) cosTable[(size_t) i] = (float) std::cos (2.0 * kPi * i / N);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxChunk, 2 };
        for (auto& c : convs) { c.prepare (spec); c.reset(); }

        for (auto& k : ready) k.setSize (1, N);
        design (d, ready);
        for (int b = 0; b < kMaxBands; ++b)
        {
            juce::AudioBuffer<float> copy (ready[b]);
            convs[b].loadImpulseResponse (std::move (copy), sr, juce::dsp::Convolution::Stereo::no,
                                          juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        }
        current = d; requested = d;
        hasReady = false;
        startThread();
    }

    int getLatency() const noexcept { return N / 2; }

    // Audio thread: ask for new kernels if the design changed.
    void request (const Design& d) noexcept
    {
        if (! (d != requested)) return;
        if (lock.tryEnter())
        {
            pending = d; hasPending = true; requested = d;
            lock.exit();
            notify();
        }
    }

    // Audio thread: swap in finished kernels (wait-free).
    void pull() noexcept
    {
        if (! hasReady.load()) return;
        if (! lock.tryEnter()) return;
        for (int b = 0; b < kMaxBands; ++b)
            if (ready[b].getNumSamples() > 0)
                convs[b].loadImpulseResponse (std::move (ready[b]), sr, juce::dsp::Convolution::Stereo::no,
                                              juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        hasReady = false;
        lock.exit();
    }

    // Splits a 2-channel chunk into per-band buffers.
    void process (const juce::dsp::AudioBlock<float>& in, juce::dsp::AudioBlock<float>* bands, int nBands) noexcept
    {
        for (int b = 0; b < nBands; ++b)
        {
            bands[b].copyFrom (in);
            juce::dsp::ProcessContextReplacing<float> ctx (bands[b]);
            convs[b].process (ctx);
        }
    }

    void reset() { for (auto& c : convs) c.reset(); }

private:
    void run() override
    {
        std::vector<juce::AudioBuffer<float>> work (kMaxBands);
        while (! threadShouldExit())
        {
            wait (-1);
            if (threadShouldExit()) break;
            Design d;
            {
                const juce::SpinLock::ScopedLockType sl (lock);
                if (! hasPending) continue;
                d = pending; hasPending = false;
            }
            for (auto& w : work) w.setSize (1, N, false, false, true);
            design (d, work);
            {
                const juce::SpinLock::ScopedLockType sl (lock);
                for (int b = 0; b < kMaxBands; ++b) { ready[b].setSize (1, N, false, false, true); ready[b].copyFrom (0, 0, work[(size_t) b], 0, 0, N); }
                hasReady = true;
            }
            if (hasPending) notify();
        }
    }

    template <typename Buffers>
    void design (const Design& d, Buffers& out) const
    {
        const int half = N / 2;
        std::vector<float> mag ((size_t) half + 1);
        for (int b = 0; b < kMaxBands; ++b)
        {
            for (int k = 0; k <= half; ++k)
            {
                const double f = (double) k * sr / N;
                double m = 1.0;
                if (b >= d.nBands) m = 0.0;
                else
                {
                    for (int j = 0; j < b; ++j) m *= 1.0 - lpMag (f, d.f[j], d.s[j]);
                    if (b < d.nBands - 1) m *= lpMag (f, d.f[b], d.s[b]);
                }
                mag[(size_t) k] = (float) m;
            }
            auto* h = out[(size_t) b].getWritePointer (0);
            for (int n = 0; n < N; ++n)
            {
                const int t = n - half;
                const int idx0 = ((t % N) + N) % N;
                double acc = mag[0];
                int idx = 0;
                for (int k = 1; k < half; ++k)
                {
                    idx += idx0; if (idx >= N) idx -= N;
                    acc += 2.0 * mag[(size_t) k] * cosTable[(size_t) idx];
                }
                acc += mag[(size_t) half] * ((t & 1) ? -1.0 : 1.0);
                const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * n / N);
                h[n] = (float) (acc / N * w);
            }
        }
    }

    static double lpMag (double f, double fc, int slope)
    {
        const double order = slope == 0 ? 1.0 : slope == 1 ? 2.0 : slope == 2 ? 4.0 : 8.0;
        return 1.0 / (1.0 + std::pow (f / std::max (fc, 1.0), order));
    }

    double sr = 44100;
    int N = 4096;
    std::vector<float> cosTable;
    juce::dsp::Convolution convs[kMaxBands];
    std::vector<juce::AudioBuffer<float>> ready = std::vector<juce::AudioBuffer<float>> (kMaxBands);
    Design pending, requested, current;
    bool hasPending = false;
    std::atomic<bool> hasReady { false };
    juce::SpinLock lock;
};
} // namespace anjis
