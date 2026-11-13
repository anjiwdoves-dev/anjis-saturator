#pragma once
#include <cmath>
#include <algorithm>

namespace anjis
{
constexpr double kPi = 3.14159265358979323846;

// Topology-preserving-transform one-pole (stable under fast modulation).
struct OnePole
{
    float g = 0.0f, s = 0.0f;
    void setCutoff (double fc, double sr) noexcept
    {
        fc = std::clamp (fc, 1.0, sr * 0.49);
        const double w = std::tan (kPi * fc / sr);
        g = (float) (w / (1.0 + w));
    }
    inline float lp (float x) noexcept
    {
        const float v = (x - s) * g;
        const float y = v + s;
        s = y + v;
        return y;
    }
    inline float hp (float x) noexcept { return x - lp (x); }
    inline float ap (float x) noexcept { return 2.0f * lp (x) - x; }
    void reset() noexcept { s = 0.0f; }
};

// TPT state-variable filter (Zavalishin / Simper form).
struct Svf
{
    float g = 0.0f, k = 1.4142f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lpOut = 0, bpOut = 0, hpOut = 0;

    void set (double fc, double sr, float kDamp) noexcept
    {
        fc = std::clamp (fc, 1.0, sr * 0.49);
        g  = (float) std::tan (kPi * fc / sr);
        k  = kDamp;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lpOut = v2; bpOut = v1; hpOut = x - k * v1 - v2;
    }
    inline float lp (float x) noexcept { tick (x); return lpOut; }
    inline float hp (float x) noexcept { tick (x); return hpOut; }
    inline float bp (float x) noexcept { tick (x); return bpOut; }
    inline float ap (float x) noexcept { tick (x); return x - 2.0f * k * bpOut; }
    void reset() noexcept { ic1 = ic2 = 0; }
};

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Simple attack/release envelope follower.
struct EnvFollower
{
    float att = 0.0f, rel = 0.0f, env = 0.0f;
    void set (double attMs, double relMs, double sr) noexcept
    {
        att = (float) std::exp (-1.0 / (std::max (attMs, 0.01) * 0.001 * sr));
        rel = (float) std::exp (-1.0 / (std::max (relMs, 0.01) * 0.001 * sr));
    }
    inline float process (float x) noexcept
    {
        x = std::abs (x);
        const float c = x > env ? att : rel;
        env = x + c * (env - x);
        return env;
    }
    void reset() noexcept { env = 0.0f; }
};
} // namespace anjis
