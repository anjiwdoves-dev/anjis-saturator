#pragma once
#include "Filters.h"
#include <array>

namespace anjis
{
// Linkwitz-Riley style crossover whose bands always sum back to an allpass
// (flat magnitude) at 6, 12, 24 and 48 dB/oct.
class MonoCrossover
{
public:
    static constexpr int kMaxBands = 6;

    void setup (int numBands, const float* freqs, const int* slopes, double sampleRate) noexcept
    {
        nBands = std::clamp (numBands, 1, kMaxBands);
        for (int i = 0; i < nBands - 1; ++i)
        {
            if (! (freqs[i] == lastF[i] && slopes[i] == lastS[i] && sampleRate == lastSr))
            {
                splits[i].set (freqs[i], sampleRate, slopes[i]);
                for (int b = 0; b < kMaxBands; ++b) comps[b][i].set (freqs[i], sampleRate, slopes[i]);
                lastF[i] = freqs[i]; lastS[i] = slopes[i];
            }
        }
        lastSr = sampleRate;
    }

    inline void process (float x, float* out) noexcept
    {
        float rest = x;
        for (int i = 0; i < nBands - 1; ++i)
        {
            splits[i].process (rest);
            out[i] = splits[i].lo;
            rest   = splits[i].hi;
        }
        out[nBands - 1] = rest;

        for (int b = 0; b < nBands - 2; ++b)
            for (int j = b + 1; j < nBands - 1; ++j)
                out[b] = comps[b][j].process (out[b]);
    }

    void reset() noexcept
    {
        for (auto& s : splits) s.reset();
        for (auto& row : comps) for (auto& c : row) c.reset();
    }

private:
    static constexpr float k1 = 1.847759f, k2 = 0.765367f, kB2 = 1.414214f;

    struct Split
    {
        int slope = 2;
        OnePole la, lb, ha, hb;
        std::array<Svf, 4> l, h;
        float lo = 0, hi = 0;

        void set (double fc, double sr, int s) noexcept
        {
            if (s != slope) reset();
            slope = s;
            la.setCutoff (fc, sr); lb.setCutoff (fc, sr); ha.setCutoff (fc, sr); hb.setCutoff (fc, sr);
            if (slope == 2) { for (int i = 0; i < 2; ++i) { l[i].set (fc, sr, kB2); h[i].set (fc, sr, kB2); } }
            if (slope == 3)
                for (int i = 0; i < 4; ++i)
                {
                    const float k = (i % 2 == 0) ? k1 : k2;
                    l[i].set (fc, sr, k); h[i].set (fc, sr, k);
                }
        }
        inline void process (float x) noexcept
        {
            switch (slope)
            {
                case 0:  lo = la.lp (x); hi = x - lo; break;
                case 1:  lo = lb.lp (la.lp (x)); hi = -hb.hp (ha.hp (x)); break;
                case 2:  lo = l[1].lp (l[0].lp (x)); hi = h[1].hp (h[0].hp (x)); break;
                default: lo = l[3].lp (l[2].lp (l[1].lp (l[0].lp (x))));
                         hi = h[3].hp (h[2].hp (h[1].hp (h[0].hp (x)))); break;
            }
        }
        void reset() noexcept { la.reset(); lb.reset(); ha.reset(); hb.reset(); for (auto& f : l) f.reset(); for (auto& f : h) f.reset(); }
    };

    struct Comp
    {
        int slope = 2;
        OnePole op;
        Svf a, b;
        void set (double fc, double sr, int s) noexcept
        {
            if (s != slope) reset();
            slope = s;
            op.setCutoff (fc, sr);
            if (slope == 2) a.set (fc, sr, kB2);
            if (slope == 3) { a.set (fc, sr, k1); b.set (fc, sr, k2); }
        }
        inline float process (float x) noexcept
        {
            switch (slope)
            {
                case 0:  return x;
                case 1:  return op.ap (x);
                case 2:  return a.ap (x);
                default: return b.ap (a.ap (x));
            }
        }
        void reset() noexcept { op.reset(); a.reset(); b.reset(); }
    };

    int nBands = 1;
    std::array<Split, kMaxBands - 1> splits;
    std::array<std::array<Comp, kMaxBands - 1>, kMaxBands> comps;
    std::array<float, kMaxBands> lastF { -1, -1, -1, -1, -1, -1 };
    std::array<int, kMaxBands> lastS { -1, -1, -1, -1, -1, -1 };
    double lastSr = 0;
};
} // namespace anjis
