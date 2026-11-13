#pragma once
#include "Knob.h"

namespace anjis::ui
{
// Interactive multiband display: spectrum, band regions, draggable crossovers.
class BandDisplay : public juce::Component, public Animated
{
public:
    std::function<void()> onBandSelected;

    explicit BandDisplay (AnjisAudioProcessor& p) : proc (p), fft (kOrder)
    {
        for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) kFft);
        for (auto& b : glow) b.speed = 0.25f;
        setHelp (*this, "Drag the lines to move crossovers. Double-click to add a band. Right-click a band or line for more.");
    }

    static float freqToNorm (float f) { return std::log (std::max (f, 20.0f) / 20.0f) / std::log (1000.0f); }
    static float normToFreq (float n) { return 20.0f * std::pow (1000.0f, n); }

    void animTick() override
    {
        ++frame;
        const int nb = (int) proc.get ("bands");
        for (int b = 0; b < kMaxBands; ++b)
        {
            const float db = gainToDb (proc.bandOutRms[b].load());
            glow[b].target = b < nb ? juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 42.0f) : 0.0f;
            glow[b].tick();
        }
        selAnim.target = (float) proc.selectedBand.load();
        selAnim.tick();
        if (frame % 2 == 0) updateSpectrum();
        repaint();
    }

    void resized() override { plot = getLocalBounds().toFloat().reduced (1).withTrimmedTop (kHeader).withTrimmedBottom (18); }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (full, 14.0f);

        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (full, 14.0f);
        g.reduceClipRegion (clip);

        const int nb = (int) proc.get ("bands");
        const int sel = proc.selectedBand.load();
        float edges[kMaxBands + 1];
        edges[0] = plot.getX(); edges[nb] = plot.getRight();
        for (int i = 0; i < nb - 1; ++i) edges[i + 1] = xForFreq (proc.get (xoverId (i)));

        bool anySolo = false;
        for (int b = 0; b < nb; ++b) anySolo |= proc.get (bandId (b, "solo")) > 0.5f;

        // band regions
        for (int b = 0; b < nb; ++b)
        {
            auto r = juce::Rectangle<float>::leftTopRightBottom (edges[b], full.getY(), edges[b + 1], full.getBottom());
            const bool muted = proc.get (bandId (b, "mute")) > 0.5f || (anySolo && proc.get (bandId (b, "solo")) < 0.5f);
            const bool isSel = b == sel;
            g.setColour (isSel ? col::lilac2 : col::panel);
            g.fillRect (r);
            if (! muted)
            {
                const float gl = glow[b].value;
                juce::ColourGradient grad (col::purple.withAlpha (0.0f), 0, plot.getY() + plot.getHeight() * (0.6f - 0.5f * gl),
                                           col::purple.withAlpha (0.04f + 0.14f * gl), 0, plot.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRect (r.withTop (plot.getY()).withBottom (plot.getBottom()));
            }
            else
            {
                g.setColour (col::panel2.withAlpha (0.85f));
                g.fillRect (r);
            }
        }

        // grid
        g.setFont (Fonts::label (10.5f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xForFreq (f);
            g.setColour (col::lineSoft);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col::ink3);
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x - 20, plot.getBottom() + 2, 40, 14), juce::Justification::centred);
        }

        // spectrum
        if (specReady)
        {
            juce::Path in, out;
            buildPath (in, specInDb, true);
            buildPath (out, specOutDb, true);
            g.setColour (col::ink3.withAlpha (0.18f)); g.fillPath (in);
            g.setColour (col::purple.withAlpha (0.12f)); g.fillPath (out);
            juce::Path outLine; buildPath (outLine, specOutDb, false);
            g.setColour (col::purple.withAlpha (0.85f));
            g.strokePath (outLine, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
        }

        // headers
        for (int b = 0; b < nb; ++b)
        {
            auto h = juce::Rectangle<float>::leftTopRightBottom (edges[b], full.getY(), edges[b + 1], full.getY() + kHeader);
            const bool isSel = b == sel;
            if (isSel) { g.setColour (col::lilac); g.fillRect (h); }
            auto t = h.reduced (10, 0);
            if (t.getWidth() < 30) continue;
            const auto styleName = styleNames()[(int) proc.get (bandId (b, "style"))];
            g.setColour (isSel ? col::purpleDk : col::ink3);
            g.setFont (Fonts::strong (11.0f));
            g.drawText (juce::String (b + 1), t.removeFromLeft (14), juce::Justification::centredLeft);
            auto btns = buttonsFor (b, edges);
            const float textRight = btns.isEmpty() ? t.getRight() : btns.getX() - 4;
            g.setColour (isSel ? col::ink : col::ink2);
            g.setFont (Fonts::strong (12.0f));
            g.drawFittedText (styleName, t.withRight (textRight).toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
            if (! btns.isEmpty()) drawMiniButtons (g, b, btns);
        }

        // drive "heat" bars on the bottom of each region
        for (int b = 0; b < nb; ++b)
        {
            const float drv = proc.modNorm[1 + b * kBandTargets].load();
            auto bar = juce::Rectangle<float>::leftTopRightBottom (edges[b] + 10, plot.getBottom() - 6, edges[b + 1] - 10, plot.getBottom() - 3);
            if (bar.getWidth() < 10) continue;
            g.setColour (col::lineSoft); g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (b == sel ? col::purple : col::mod); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * drv), 1.5f);
        }

        // crossovers
        for (int i = 0; i < nb - 1; ++i)
        {
            const float x = edges[i + 1];
            const bool active = (i == hoverX || i == dragX);
            g.setColour (active ? col::purple : col::line);
            g.fillRect (juce::Rectangle<float> (x - (active ? 1.0f : 0.5f), full.getY() + kHeader, active ? 2.0f : 1.0f, plot.getBottom() - kHeader));

            // modulated ghost position
            if (auto* xp = proc.param (xoverId (i)))
            {
                const float mf = xp->convertFrom0to1 (proc.modNorm[1 + kMaxBands * kBandTargets + i].load());
                const float mx = xForFreq (mf);
                if (std::abs (mx - x) > 1.5f)
                {
                    g.setColour (col::mod);
                    g.drawVerticalLine ((int) mx, plot.getY(), plot.getBottom());
                }
            }

            const float f = proc.get (xoverId (i));
            const juce::String txt = f >= 1000 ? juce::String (f / 1000.0f, f >= 10000 ? 1 : 2) + "k" : juce::String ((int) f);
            g.setFont (Fonts::strong (10.5f));
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), txt) + 14;
            auto pill = juce::Rectangle<float> (tw, 18).withCentre ({ x, plot.getY() + 14 });
            g.setColour (active ? col::purple : col::panel);
            g.fillRoundedRectangle (pill, 9);
            g.setColour (active ? col::purple : col::line);
            g.drawRoundedRectangle (pill, 9, 1);
            g.setColour (active ? col::panel : col::ink2);
            g.drawText (txt, pill, juce::Justification::centred);
            const int slope = (int) proc.get (slopeId (i));
            if (active)
            {
                g.setColour (col::purpleDk);
                g.setFont (Fonts::label (10.0f));
                const char* sl[] = { "6", "12", "24", "48" };
                g.drawText (juce::String (sl[slope]) + " dB/oct", juce::Rectangle<float> (70, 14).withCentre ({ x, pill.getBottom() + 10 }), juce::Justification::centred);
            }
        }

        if (nb == 1)
        {
            g.setColour (col::ink3);
            g.setFont (Fonts::label (12.5f));
            g.drawText ("Double-click anywhere to split into bands", plot.withTrimmedBottom (plot.getHeight() * 0.45f), juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = crossoverAt (e.position.x);
        if (h != hoverX) { hoverX = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hoverX = -1; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        proc.undo.beginNewTransaction();
        const int nb = (int) proc.get ("bands");
        const int xi = crossoverAt (e.position.x);
        if (e.mods.isPopupMenu())
        {
            if (xi >= 0) slopeMenu (xi); else bandMenu (bandAt (e.position.x));
            return;
        }
        if (xi >= 0)
        {
            dragX = xi;
            if (auto* p = proc.param (xoverId (xi))) p->beginChangeGesture();
            return;
        }
        const int b = bandAt (e.position.x);
        if (b < 0 || b >= nb) return;

        // mini buttons in header
        float edges[kMaxBands + 1]; fillEdges (edges, nb);
        auto btns = buttonsFor (b, edges);
        if (! btns.isEmpty() && btns.contains (e.position))
        {
            const int which = (int) ((e.position.x - btns.getX()) / (btns.getWidth() / 3.0f));
            if (which == 0) toggle (bandId (b, "solo"));
            else if (which == 1) toggle (bandId (b, "mute"));
            else if (nb > 1) proc.removeBand (b);
            if (onBandSelected) onBandSelected();
            return;
        }
        proc.selectedBand = b;
        if (onBandSelected) onBandSelected();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragX < 0) return;
        const int nb = (int) proc.get ("bands");
        float f = freqForX (e.position.x);
        const float lo = dragX > 0 ? proc.get (xoverId (dragX - 1)) * 1.06f : 20.0f;
        const float hi = dragX < nb - 2 ? proc.get (xoverId (dragX + 1)) / 1.06f : 20000.0f;
        f = juce::jlimit (lo, hi, f);
        if (auto* p = proc.param (xoverId (dragX))) p->setValueNotifyingHost (p->convertTo0to1 (f));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragX >= 0) if (auto* p = proc.param (xoverId (dragX))) p->endChangeGesture();
        dragX = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (crossoverAt (e.position.x) >= 0 || e.position.y < kHeader) return;
        proc.insertBandAt (freqForX (e.position.x));
        if (onBandSelected) onBandSelected();
    }

private:
    static constexpr int kOrder = 12, kFft = 1 << kOrder;
    static constexpr float kHeader = 30.0f;

    float xForFreq (float f) const { return plot.getX() + freqToNorm (f) * plot.getWidth(); }
    float freqForX (float x) const { return normToFreq (juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth())); }

    void fillEdges (float* edges, int nb) const
    {
        edges[0] = plot.getX(); edges[nb] = plot.getRight();
        for (int i = 0; i < nb - 1; ++i) edges[i + 1] = xForFreq (proc.get (xoverId (i)));
    }

    int crossoverAt (float x) const
    {
        const int nb = (int) proc.get ("bands");
        for (int i = 0; i < nb - 1; ++i)
            if (std::abs (xForFreq (proc.get (xoverId (i))) - x) < 7.0f) return i;
        return -1;
    }
    int bandAt (float x) const
    {
        const int nb = (int) proc.get ("bands");
        int b = 0;
        while (b < nb - 1 && x > xForFreq (proc.get (xoverId (b)))) ++b;
        return b;
    }

    juce::Rectangle<float> buttonsFor (int b, const float* edges) const
    {
        const float w = edges[b + 1] - edges[b];
        if (w < 120) return {};
        return { edges[b + 1] - 70.0f, 6.0f, 60.0f, 18.0f };
    }

    void drawMiniButtons (juce::Graphics& g, int b, juce::Rectangle<float> r)
    {
        const bool solo = proc.get (bandId (b, "solo")) > 0.5f, mute = proc.get (bandId (b, "mute")) > 0.5f;
        const float w = r.getWidth() / 3.0f;
        const juce::String t[] = { "S", "M", juce::String::fromUTF8 ("\xc3\x97") };
        const bool on[] = { solo, mute, false };
        for (int i = 0; i < 3; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + (float) i * w, r.getY(), w, r.getHeight()).reduced (1.5f, 0);
            const juce::Colour onCol = i == 1 ? col::warn : col::purple;
            g.setColour (on[i] ? onCol : col::panel.withAlpha (0.8f));
            g.fillRoundedRectangle (c, 5);
            g.setColour (on[i] ? onCol : col::line);
            g.drawRoundedRectangle (c, 5, 1);
            g.setColour (on[i] ? col::panel : col::ink2);
            g.setFont (Fonts::strong (10.5f));
            g.drawText (t[i], c, juce::Justification::centred);
        }
    }

    void toggle (const juce::String& id) { proc.set (id, proc.get (id) > 0.5f ? 0.0f : 1.0f); }

    void slopeMenu (int xi)
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Crossover " + juce::String (xi + 1) + " slope");
        const int cur = (int) proc.get (slopeId (xi));
        const char* names[] = { "6 dB/oct  (gentle)", "12 dB/oct", "24 dB/oct  (default)", "48 dB/oct  (steep)" };
        for (int i = 0; i < 4; ++i) m.addItem (i + 1, names[i], true, i == cur);
        juce::Component::SafePointer<BandDisplay> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, xi] (int r) { if (safe && r > 0) safe->proc.set (slopeId (xi), (float) (r - 1)); });
    }

    void bandMenu (int b)
    {
        const int nb = (int) proc.get ("bands");
        if (b < 0 || b >= nb) return;
        proc.selectedBand = b;
        if (onBandSelected) onBandSelected();
        juce::PopupMenu m, copyTo;
        m.addSectionHeader ("Band " + juce::String (b + 1));
        m.addItem (1, "Solo", true, proc.get (bandId (b, "solo")) > 0.5f);
        m.addItem (2, "Mute", true, proc.get (bandId (b, "mute")) > 0.5f);
        m.addItem (3, "Bypass saturation", true, proc.get (bandId (b, "byp")) > 0.5f);
        m.addSeparator();
        for (int i = 0; i < nb; ++i) if (i != b) copyTo.addItem (20 + i, "Band " + juce::String (i + 1));
        m.addSubMenu ("Copy settings to", copyTo, nb > 1);
        m.addItem (4, "Remove band", nb > 1);
        juce::Component::SafePointer<BandDisplay> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, b] (int r)
        {
            if (! safe || r == 0) return;
            auto& p = safe->proc;
            p.undo.beginNewTransaction();
            if (r == 1) safe->toggle (bandId (b, "solo"));
            if (r == 2) safe->toggle (bandId (b, "mute"));
            if (r == 3) safe->toggle (bandId (b, "byp"));
            if (r == 4) p.removeBand (b);
            if (r >= 20) p.copyBand (b, r - 20);
            if (safe->onBandSelected) safe->onBandSelected();
        });
    }

    void updateSpectrum()
    {
        const double sr = proc.getCurrentSampleRate();
        auto doOne = [&] (const VisualRing& ring, std::vector<float>& db)
        {
            ring.read (fftData.data(), kFft);
            for (int i = 0; i < kFft; ++i) fftData[(size_t) i] *= window[(size_t) i];
            std::fill (fftData.begin() + kFft, fftData.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
            const int cols = kCols;
            db.resize ((size_t) cols, -100.0f);
            for (int c = 0; c < cols; ++c)
            {
                const float f0 = normToFreq ((float) c / cols), f1 = normToFreq ((float) (c + 1) / cols);
                int b0 = (int) (f0 / sr * kFft), b1 = std::max (b0 + 1, (int) (f1 / sr * kFft));
                b0 = juce::jlimit (1, kFft / 2 - 1, b0); b1 = juce::jlimit (b0 + 1, kFft / 2, b1);
                float m = 0;
                for (int k = b0; k < b1; ++k) m = std::max (m, fftData[(size_t) k]);
                const float fc = std::sqrt (f0 * f1);
                float v = gainToDb (m * 4.0f / kFft) + 4.5f * std::log2 (fc / 1000.0f); // tilt for musical view
                auto& s = db[(size_t) c];
                s = v > s ? s + (v - s) * 0.6f : s + (v - s) * 0.12f;
            }
        };
        doOne (proc.specIn, specInDb);
        doOne (proc.specOut, specOutDb);
        specReady = true;
    }

    void buildPath (juce::Path& p, const std::vector<float>& db, bool closed) const
    {
        const int cols = (int) db.size();
        auto yFor = [&] (float d) { return juce::jmap (juce::jlimit (-90.0f, 0.0f, d), -90.0f, 0.0f, plot.getBottom(), plot.getY() + 34.0f); };
        if (closed) p.startNewSubPath (plot.getX(), plot.getBottom());
        for (int c = 0; c < cols; ++c)
        {
            const float x = plot.getX() + plot.getWidth() * ((float) c + 0.5f) / (float) cols;
            const float y = yFor (db[(size_t) c]);
            if (! closed && c == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (plot.getRight(), plot.getBottom()); p.closeSubPath(); }
    }

    static constexpr int kCols = 160;
    AnjisAudioProcessor& proc;
    juce::dsp::FFT fft;
    std::array<float, kFft> window {};
    std::vector<float> fftData = std::vector<float> (kFft * 2);
    std::vector<float> specInDb, specOutDb;
    bool specReady = false;
    juce::Rectangle<float> plot;
    int hoverX = -1, dragX = -1, frame = 0;
    Anim glow[kMaxBands], selAnim;
};
} // namespace anjis::ui
