#pragma once
#include "Filters.h"
#include <cstdint>

namespace anjis
{
inline float ftanh (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline float softClip (float x) noexcept
{
    if (x >= 1.0f) return 1.0f;
    if (x <= -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}
inline float hardClip (float x) noexcept { return std::clamp (x, -1.0f, 1.0f); }

// One channel of the saturation stage. All 32 styles live here.
class Shaper
{
public:
    void setSampleRate (double newSr) noexcept
    {
        sr = newSr;
        pre.setCutoff (3000, sr);   post.setCutoff (5000, sr);  cass.setCutoff (6500, sr);
        iron.setCutoff (180, sr);   ampHp.setCutoff (90, sr);   ampHp2.setCutoff (220, sr);
        cabA.setCutoff (6500, sr);  cabB.setCutoff (4800, sr);  bassLp.setCutoff (250, sr);
        excite.setCutoff (3500, sr); subLp.setCutoff (220, sr); subLp2.setCutoff (160, sr);
        zcLp.setCutoff (400, sr);
        envFast.set (0.3, 30.0, sr);
        envGlue.set (5.0, 90.0, sr);
        envSmash.set (0.5, 40.0, sr);
    }

    void reset() noexcept
    {
        for (auto* f : { &pre, &post, &cass, &iron, &ampHp, &ampHp2, &cabA, &cabB, &bassLp, &excite, &subLp, &subLp2, &zcLp })
            f->reset();
        envFast.reset(); envGlue.reset(); envSmash.reset();
        phase = 0; held = 0; holdPhase = 1; flip = 1.0f; lastZ = 0;
    }

    // raw  : band signal (plus feedback) before drive
    // amt  : drive 0..1
    // gain : linear drive gain, comp : loudness compensation for gain-based styles
    inline float process (int style, float raw, float amt, float gain, float comp) noexcept
    {
        const float g = raw * gain;
        switch (style)
        {
            case 0:  return (ftanh (g + 0.25f) - 0.244918f) * comp;
            case 1:  return softClip (g * 0.66f) * 1.25f * comp;
            case 2:  { const float a = ftanh (1.6f * g + 0.4f) - 0.379949f; return ftanh (1.3f * a) * comp; }
            case 3:  return (g >= 0 ? (1.0f - std::exp (-g)) : -0.65f * (1.0f - std::exp (1.6f * g))) * comp;
            case 4:  return 0.63662f * std::atan (1.6f * g) * comp;
            case 5:  { const float hi = g - pre.lp (g); const float y = ftanh (0.9f * (g + 0.5f * hi)); return (y - 0.33f * (y - post.lp (y))) * comp; }
            case 6:  { const float hi = g - pre.lp (g); float y = ftanh (1.4f * g + 0.6f * hi); y *= 1.0f - 0.2f * std::abs (y); return (y - 0.45f * (y - post.lp (y))) * comp; }
            case 7:  { const float y = ftanh (0.8f * g + 0.08f); return cass.lp (y) * comp; }
            case 8:  { const float lo = iron.lp (g); return (0.8f * ftanh (1.6f * lo) + softClip (0.8f * (g - lo))) * comp; }
            case 9:  { const float lo = iron.lp (g); return (0.7f * ftanh (3.2f * lo + 0.15f) + 0.8f * ftanh (g - lo)) * comp; }
            case 10: { const float t = ftanh (g); return ftanh (0.8f * g + 0.12f * t * t) * comp; }
            case 11: { const float u = ampHp.hp (g); return cabA.lp (ftanh (1.2f * u + 0.1f) - 0.099668f) * comp; }
            case 12: { const float u = ampHp2.hp (g); const float s1 = ftanh (2.0f * u + 0.2f) - 0.197375f; return cabB.lp (ftanh (2.5f * s1)) * comp * 0.9f; }
            case 13: { const float u = ampHp2.hp (g); const float s1 = ftanh (3.0f * u + 0.3f) - 0.291313f;
                       const float s2 = ftanh (4.0f * ampHp.hp (s1)); return cabB.lp (ftanh (3.0f * s2)) * comp * 0.8f; }
            case 14: { const float lo = bassLp.lp (g); return cabA.lp (softClip (0.6f * lo) + 0.5f * ftanh (3.0f * (g - lo))) * comp; }
            case 15: { const float e = envFast.process (raw); const float gate = std::clamp (e * 300.0f - 0.2f, 0.0f, 1.0f);
                       const float u = 2.0f * g + 0.3f; const float y = u > 0 ? ftanh (3.0f * u) : 0.7f * ftanh (0.8f * u);
                       return y * gate * comp * 0.8f; }
            case 16: return softClip (g) * comp;
            case 17: return hardClip (g) * comp;
            case 18: return (g > 0 ? 0.7f * ftanh (g / 0.7f) : 0.9f * ftanh (g * 0.55f)) * comp * 1.2f;
            case 19: return std::abs (ftanh (g)) * 1.4f * comp;
            case 20: return std::max (ftanh (g), 0.0f) * 1.6f * comp;
            case 21: { const float hp = raw - excite.lp (raw); return raw + ftanh (hp * gain * 2.0f) * 0.25f * amt; }
            case 22: return std::sin (g * 1.5707963f) * comp;
            case 23: { float t = (g + 1.0f) * 0.25f; t -= std::floor (t); return (1.0f - 4.0f * std::abs (t - 0.5f)) * comp; }
            case 24: { const float u = ftanh (g); return (0.6f * u + 0.25f * (2 * u * u - 1) + 0.25f * (4 * u * u * u - 3 * u)) * comp; }
            case 25: { const float bits = 16.0f - amt * 14.5f; const float q = std::pow (2.0f, bits - 1.0f);
                       return std::round (raw * (1.0f + amt) * q) / q; }
            case 26: { const double rate = 44100.0 * std::pow (2.0, -amt * 6.5);
                       holdPhase += rate / sr; if (holdPhase >= 1.0) { holdPhase -= std::floor (holdPhase); held = raw; }
                       return held * (1.0f + 0.5f * amt); }
            case 27: { const double f = 30.0 * std::pow (2.0, amt * 7.0); phase += f / sr; if (phase >= 1.0) phase -= 1.0;
                       return softClip (raw * 1.6f) * (float) std::sin (phase * 6.283185307) * 1.3f; }
            case 28: { const float u = ftanh (g); const float a = std::abs (u);
                       const float rattle = a > 0.55f ? noise() * (a - 0.55f) * 1.6f : 0.0f;
                       return (0.85f * u + rattle) * comp; }
            case 29: { const float z = zcLp.lp (raw);
                       if (lastZ <= 0 && z > 0) flip = -flip; lastZ = z;
                       const float e = envFast.process (raw);
                       const float sub = subLp2.lp (subLp.lp (flip * e));
                       return raw * 0.7f + sub * (1.0f + 2.5f * amt); }
            case 30: { const float e = envGlue.process (g); const float gr = 1.0f / (1.0f + std::max (e - 0.5f, 0.0f) * 2.0f);
                       return ftanh (g * gr * 1.2f) * comp; }
            default: { const float e = envSmash.process (g); const float gr = 1.0f / (1.0f + std::max (e - 0.2f, 0.0f) * 8.0f);
                       return ftanh (hardClip (g * gr * 2.0f) * 1.2f) * comp; }
        }
    }

    // Static transfer curve for drawing (no state, no filters).
    static float curve (int style, float x, float amt, float gain) noexcept
    {
        const float g = x * gain;
        switch (style)
        {
            case 0:  return ftanh (g + 0.25f) - 0.244918f;
            case 1:  return softClip (g * 0.66f) * 1.25f;
            case 2:  return ftanh (1.3f * (ftanh (1.6f * g + 0.4f) - 0.379949f));
            case 3:  return g >= 0 ? (1.0f - std::exp (-g)) : -0.65f * (1.0f - std::exp (1.6f * g));
            case 4:  return 0.63662f * std::atan (1.6f * g);
            case 5:  return ftanh (0.9f * g);
            case 6:  { float y = ftanh (1.4f * g); return y * (1.0f - 0.2f * std::abs (y)); }
            case 7:  return ftanh (0.8f * g + 0.08f);
            case 8:  return 0.8f * ftanh (1.6f * g);
            case 9:  return 0.7f * ftanh (3.2f * g + 0.15f);
            case 10: { const float t = ftanh (g); return ftanh (0.8f * g + 0.12f * t * t); }
            case 11: return ftanh (1.2f * g + 0.1f) - 0.099668f;
            case 12: return 0.9f * ftanh (2.5f * (ftanh (2.0f * g + 0.2f) - 0.197375f));
            case 13: return 0.8f * ftanh (3.0f * ftanh (4.0f * (ftanh (3.0f * g + 0.3f) - 0.291313f)));
            case 14: return softClip (0.6f * g);
            case 15: { const float u = 2.0f * g + 0.3f; return 0.8f * (u > 0 ? ftanh (3.0f * u) : 0.7f * ftanh (0.8f * u)); }
            case 16: return softClip (g);
            case 17: return hardClip (g);
            case 18: return 1.2f * (g > 0 ? 0.7f * ftanh (g / 0.7f) : 0.9f * ftanh (g * 0.55f));
            case 19: return std::abs (ftanh (g)) * 1.4f;
            case 20: return std::max (ftanh (g), 0.0f) * 1.6f;
            case 21: return x;
            case 22: return std::sin (g * 1.5707963f);
            case 23: { float t = (g + 1.0f) * 0.25f; t -= std::floor (t); return 1.0f - 4.0f * std::abs (t - 0.5f); }
            case 24: { const float u = ftanh (g); return 0.6f * u + 0.25f * (2 * u * u - 1) + 0.25f * (4 * u * u * u - 3 * u); }
            case 25: { const float q = std::pow (2.0f, 15.0f - amt * 14.5f - 8.0f); return std::round (x * q) / q; }
            case 26: return x;
            case 27: return softClip (x * 1.6f);
            case 28: return 0.85f * ftanh (g);
            case 29: return x * 0.7f;
            case 30: return ftanh (g * 1.2f);
            default: return ftanh (hardClip (g * 2.0f) * 1.2f);
        }
    }

    static bool usesGain (int style) noexcept
    {
        return ! (style == 21 || style == 25 || style == 26 || style == 27 || style == 29);
    }

private:
    inline float noise() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xffffff) / 8388608.0f - 1.0f;
    }

    double sr = 44100.0;
    OnePole pre, post, cass, iron, ampHp, ampHp2, cabA, cabB, bassLp, excite, subLp, subLp2, zcLp;
    EnvFollower envFast, envGlue, envSmash;
    double phase = 0, holdPhase = 1;
    float held = 0, flip = 1.0f, lastZ = 0;
    uint32_t rng = 0x9e3779b9u;
};
} // namespace anjis
