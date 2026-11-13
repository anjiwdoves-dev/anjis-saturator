#pragma once
#include "Knob.h"

namespace anjis::ui
{
inline void drawMiniCurve (juce::Graphics& g, juce::Rectangle<float> r, int style, float drive, juce::Colour c, float stroke = 1.6f)
{
    const float gain = dbToGain (drive * 0.4f);
    float mx = 0.0001f;
    constexpr int N = 48;
    float ys[N + 1];
    for (int i = 0; i <= N; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / N;
        ys[i] = Shaper::curve (style, x, drive * 0.01f, gain);
        mx = std::max (mx, std::abs (ys[i]));
    }
    juce::Path p;
    for (int i = 0; i <= N; ++i)
    {
        const float px = r.getX() + r.getWidth() * (float) i / N;
        const float py = r.getCentreY() - ys[i] / mx * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Shows the band's transfer curve with a live "ball" riding it, or a scope.
class CurveView : public juce::Component, public Animated
{
public:
    explicit CurveView (AnjisAudioProcessor& p) : proc (p)
    {
        setHelp (*this, "Live view of the selected band. Click to switch between the transfer curve and the scope.");
        level.speed = 0.35f;
    }

    void animTick() override
    {
        const int b = proc.selectedBand.load();
        level.target = juce::jlimit (0.0f, 1.0f, proc.bandInPeak[b].load());
        level.tick();
        trail[(size_t) trailPos] = level.value; trailPos = (trailPos + 1) % (int) trail.size();
        modeAnim.target = scope ? 1.0f : 0.0f; modeAnim.tick();
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { scope = ! scope; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 10);
        g.setColour (col::lineSoft); g.drawRoundedRectangle (r.reduced (0.5f), 10, 1);

        auto tabs = r.removeFromTop (24).reduced (8, 4);
        g.setFont (Fonts::strong (10.5f));
        const float tw = tabs.getWidth() * 0.5f;
        auto pill = juce::Rectangle<float> (tabs.getX() + modeAnim.value * tw, tabs.getY(), tw, tabs.getHeight());
        g.setColour (col::lilac); g.fillRoundedRectangle (pill, 6);
        g.setColour (scope ? col::ink3 : col::purpleDk); g.drawText ("CURVE", tabs.withWidth (tw), juce::Justification::centred);
        g.setColour (scope ? col::purpleDk : col::ink3); g.drawText ("SCOPE", tabs.withTrimmedLeft (tw), juce::Justification::centred);

        auto area = r.reduced (12, 8);
        const int b = proc.selectedBand.load();
        const int style = (int) proc.get (bandId (b, "style"));
        const float drive = proc.param (bandId (b, "drive"))->convertFrom0to1 (proc.modNorm[1 + b * kBandTargets].load());

        if (! scope)
        {
            auto sq = area.withSizeKeepingCentre (std::min (area.getWidth(), area.getHeight()), std::min (area.getWidth(), area.getHeight()));
            g.setColour (col::line);
            g.drawHorizontalLine ((int) sq.getCentreY(), sq.getX(), sq.getRight());
            g.drawVerticalLine ((int) sq.getCentreX(), sq.getY(), sq.getBottom());
            juce::Path diag; diag.startNewSubPath (sq.getBottomLeft()); diag.lineTo (sq.getTopRight());
            const float dash[] = { 3, 4 };
            g.setColour (col::ink3.withAlpha (0.5f));
            juce::PathStrokeType (1.0f).createDashedStroke (diag, diag, dash, 2);
            g.fillPath (diag);
            drawMiniCurve (g, sq, style, drive, col::purple, 2.2f);

            // live ball with trail
            const float gain = dbToGain (drive * 0.4f);
            float mx = 0.0001f;
            for (int i = 0; i <= 48; ++i) mx = std::max (mx, std::abs (Shaper::curve (style, -1.0f + (float) i / 24.0f, drive * 0.01f, gain)));
            auto toPt = [&] (float x) {
                const float y = Shaper::curve (style, x, drive * 0.01f, gain) / mx;
                return juce::Point<float> (sq.getCentreX() + x * sq.getWidth() * 0.5f, sq.getCentreY() - y * sq.getHeight() * 0.5f);
            };
            const int n = (int) trail.size();
            for (int i = 0; i < n; ++i)
            {
                const float v = trail[(size_t) ((trailPos + i) % n)];
                const float a = (float) i / (float) n;
                g.setColour (col::mod.withAlpha (a * 0.5f));
                g.fillEllipse (juce::Rectangle<float> (4 + a * 3, 4 + a * 3).withCentre (toPt (v)));
            }
            const auto pt = toPt (level.value);
            g.setColour (col::purple.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (22, 22).withCentre (pt));
            g.setColour (col::purpleDk);
            g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre (pt));
            g.setColour (col::panel);
            g.fillEllipse (juce::Rectangle<float> (4, 4).withCentre (pt));
        }
        else
        {
            constexpr int N = 1024;
            float in[N + 512], out[N + 512];
            proc.scopeIn.read (in, N + 512);
            proc.scopeOut.read (out, N + 512);
            int start = 0;
            for (int i = 1; i < 512; ++i) if (out[i - 1] <= 0 && out[i] > 0) { start = i; break; }
            float mx = 0.05f;
            for (int i = 0; i < N; ++i) mx = std::max ({ mx, std::abs (in[start + i]), std::abs (out[start + i]) });
            auto draw = [&] (const float* d, juce::Colour c, float w)
            {
                juce::Path p;
                for (int i = 0; i < N; i += 2)
                {
                    const float x = area.getX() + area.getWidth() * (float) i / N;
                    const float y = area.getCentreY() - d[start + i] / mx * area.getHeight() * 0.48f;
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (c); g.strokePath (p, juce::PathStrokeType (w));
            };
            g.setColour (col::line); g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
            draw (in, col::ink3.withAlpha (0.6f), 1.2f);
            draw (out, col::purple, 1.8f);
        }
    }

private:
    AnjisAudioProcessor& proc;
    bool scope = false;
    Anim level, modeAnim;
    std::array<float, 14> trail {};
    int trailPos = 0;
};

// Full-width overlay for picking a distortion style.
class StylePicker : public juce::Component, public Animated
{
public:
    std::function<void()> onClose;

    explicit StylePicker (AnjisAudioProcessor& p) : proc (p)
    {
        for (auto& s : styles()) if (! cats.contains (s.category)) cats.add (s.category);
    }

    void open (int bandIndex) { band = bandIndex; appear.snap (0); appear.target = 1; setVisible (true); toFront (true); }
    void close() { appear.target = 0; closing = true; }

    void animTick() override
    {
        if (appear.tick()) repaint();
        if (closing && appear.value < 0.02f) { closing = false; setVisible (false); if (onClose) onClose(); }
        if (isVisible() && hover >= 0) { phase += 0.05f; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const float a = appear.value;
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel.withAlpha (a));
        g.fillRoundedRectangle (r.translated (0, (1.0f - a) * 12.0f), 14);
        g.setColour (col::line.withAlpha (a));
        g.drawRoundedRectangle (r.reduced (0.5f).translated (0, (1.0f - a) * 12.0f), 14, 1);

        g.setOpacity (a);
        const int cur = (int) proc.get (bandId (band, "style"));
        auto content = r.reduced (16, 12).translated (0, (1.0f - a) * 12.0f);
        auto head = content.removeFromTop (22);
        g.setFont (Fonts::title (15)); g.setColour (col::ink.withAlpha (a));
        g.drawText ("Choose a style for band " + juce::String (band + 1), head, juce::Justification::centredLeft);
        g.setFont (Fonts::label (11.5f)); g.setColour (col::ink3.withAlpha (a));
        g.drawText ("click outside to close", head, juce::Justification::centredRight);
        content.removeFromTop (8);

        auto foot = content.removeFromBottom (24);
        {
            const int shownStyle = hover >= 0 ? hover : cur;
            const auto& info = styles()[(size_t) shownStyle];
            g.setFont (Fonts::strong (13)); g.setColour (col::purpleDk.withAlpha (a));
            const juce::String nm (info.name);
            g.drawText (nm, foot, juce::Justification::centredLeft);
            const int nw = juce::GlyphArrangement::getStringWidthInt (Fonts::strong (13), nm) + 12;
            g.setFont (Fonts::label (13)); g.setColour (col::ink2.withAlpha (a));
            g.drawText (info.help, foot.withTrimmedLeft ((float) nw), juce::Justification::centredLeft);
        }
        const float colW = content.getWidth() / (float) cats.size();
        for (int ci = 0; ci < cats.size(); ++ci)
        {
            auto colR = juce::Rectangle<float> (content.getX() + (float) ci * colW, content.getY(), colW, content.getHeight()).reduced (4, 0);
            g.setFont (Fonts::strong (10.5f)); g.setColour (col::ink3.withAlpha (a));
            g.drawText (cats[ci].toUpperCase(), colR.removeFromTop (18), juce::Justification::centredLeft);
            for (int s = 0; s < (int) styles().size(); ++s)
            {
                if (cats[ci] != styles()[(size_t) s].category) continue;
                auto cell = colR.removeFromTop (cellH).reduced (0, 2);
                const bool isCur = s == cur, isHover = s == hover;
                g.setColour ((isCur ? col::purple : isHover ? col::lilac : col::panel2).withAlpha (a));
                g.fillRoundedRectangle (cell, 8);
                auto icon = cell.removeFromLeft (34).reduced (7, 9);
                const float drv = isHover ? 25.0f + 25.0f * std::sin (phase) : 18.0f;
                drawMiniCurve (g, icon, s, drv, (isCur ? col::panel : col::purple).withAlpha (a), 1.5f);
                g.setFont (Fonts::strong (12.0f));
                g.setColour ((isCur ? col::panel : col::ink).withAlpha (a));
                g.drawFittedText (styles()[(size_t) s].name, cell.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
                cellRects[(size_t) s] = cell.withLeft (cell.getX() - 34);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (int s = 0; s < (int) styles().size(); ++s) if (cellRects[(size_t) s].contains (e.position)) h = s;
        if (h != hover)
        {
            hover = h;
            phase = 0;
            if (h >= 0) setHelp (*this, styles()[(size_t) h].help);
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int s = 0; s < (int) styles().size(); ++s)
            if (cellRects[(size_t) s].contains (e.position))
            {
                proc.undo.beginNewTransaction ("Change style");
                proc.set (bandId (band, "style"), (float) s);
                close();
                return;
            }
    }

private:
    AnjisAudioProcessor& proc;
    juce::StringArray cats;
    int band = 0, hover = -1;
    float phase = 0;
    bool closing = false;
    Anim appear;
    static constexpr float cellH = 32.0f;
    std::array<juce::Rectangle<float>, 32> cellRects;
};

// Controls for the selected band.
class BandPanel : public juce::Component, public Animated
{
public:
    std::function<void (int)> onOpenStylePicker;
    std::function<void (int)> onBandMenu;

    explicit BandPanel (AnjisAudioProcessor& p)
        : proc (p), drive (p, "Drive", true), fb (p, "Feedback"), fbf (p, "FB Freq"), dyn (p, "Dynamics"),
          tone (p, "Tone"), mix (p, "Mix"), lvl (p, "Level"), width (p, "Width"),
          channels (p, { "Stereo", "Mid", "Side", "L", "R" }), bypass (p, "Bypass"), curve (p)
    {
        for (auto* k : knobs()) addAndMakeVisible (k);
        addAndMakeVisible (channels);
        addAndMakeVisible (bypass);
        addAndMakeVisible (curve);
        addAndMakeVisible (menuBtn);
        menuBtn.setButtonText ("Band presets");
        menuBtn.onClick = [this] { if (onBandMenu) onBandMenu (band); };

        setHelp (drive, "Drive: how hard the band hits the saturator. Auto Gain keeps the loudness steady while you push it.");
        setHelp (fb, "Feedback: routes the saturated signal back in, ringing at FB Freq. High values resonate and howl.");
        setHelp (fbf, "Feedback frequency: the pitch the feedback rings at.");
        setHelp (dyn, "Dynamics: right makes loud parts louder (more punch), left squashes them (more density).");
        setHelp (tone, "Tone: tilts the saturated sound darker (left) or brighter (right).");
        setHelp (mix, "Mix: blend between the clean band and the saturated band.");
        setHelp (lvl, "Level: output volume of this band.");
        setHelp (width, "Width: stereo width of this band. 0% is mono, 200% is extra wide.");
        setHelp (channels, "Choose which part of the stereo image gets saturated: both, middle, sides, left or right.");
        setHelp (bypass, "Bypass the saturation on this band (the band still passes through).");
        setHelp (menuBtn, "Save, load, copy and paste band settings.");
        setBand (0);
    }

    void setBand (int b)
    {
        band = b;
        drive.setParam (bandId (b, "drive")); fb.setParam (bandId (b, "fb")); fbf.setParam (bandId (b, "fbf"));
        dyn.setParam (bandId (b, "dyn")); tone.setParam (bandId (b, "tone")); mix.setParam (bandId (b, "mix"));
        lvl.setParam (bandId (b, "lvl")); width.setParam (bandId (b, "width"));
        channels.setParam (bandId (b, "ch")); bypass.setParam (bandId (b, "byp"));
        repaint();
    }

    void animTick() override
    {
        const int s = (int) proc.get (bandId (band, "style"));
        styleHover.target = styleCard.contains (getMouseXYRelative().toFloat()) && isMouseOver (true) ? 1.0f : 0.0f;
        bool d = styleHover.tick();
        if (s != lastStyle) { lastStyle = s; d = true; }
        const float dr = proc.modNorm[1 + band * kBandTargets].load();
        if (std::abs (dr - lastDrive) > 0.001f) { lastDrive = dr; d = true; }
        if (d) repaint (styleCard.toNearestInt().expanded (2));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (r, 14);

        // title
        const int nb = (int) proc.get ("bands");
        const float lo = band == 0 ? 20.0f : proc.get (xoverId (band - 1));
        const float hi = band == nb - 1 ? 20000.0f : proc.get (xoverId (band));
        auto fmt = [] (float f) { return f >= 1000 ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz"; };
        g.setFont (Fonts::title (14)); g.setColour (col::ink);
        g.drawText ("Band " + juce::String (band + 1), titleArea, juce::Justification::centredLeft);
        g.setFont (Fonts::label (12)); g.setColour (col::ink3);
        g.drawText (fmt (lo) + "  to  " + fmt (hi), titleArea.withTrimmedLeft (64), juce::Justification::centredLeft);

        // style card
        const auto& st = styles()[(size_t) juce::jlimit (0, 31, (int) proc.get (bandId (band, "style")))];
        g.setColour (col::lilac2.interpolatedWith (col::lilac, styleHover.value));
        g.fillRoundedRectangle (styleCard, 12);
        g.setColour (col::lilac.interpolatedWith (col::mod, styleHover.value));
        g.drawRoundedRectangle (styleCard.reduced (0.5f), 12, 1);
        auto sc = styleCard.reduced (14, 8);
        auto icon = sc.removeFromRight (64).reduced (4, 6);
        const float drv = proc.param (bandId (band, "drive"))->convertFrom0to1 (proc.modNorm[1 + band * kBandTargets].load());
        drawMiniCurve (g, icon, (int) proc.get (bandId (band, "style")), drv, col::purple, 2.0f);
        g.setFont (Fonts::strong (10)); g.setColour (col::purple);
        g.drawText (juce::String (st.category).toUpperCase(), sc.removeFromTop (16), juce::Justification::centredLeft);
        g.setFont (Fonts::title (19)); g.setColour (col::ink);
        g.drawText (st.name, sc.removeFromTop (26), juce::Justification::centredLeft);
        g.setFont (Fonts::label (11)); g.setColour (col::ink2);
        g.drawText ("change style", sc, juce::Justification::centredLeft);

        // group captions
        g.setFont (Fonts::strong (10)); g.setColour (col::ink3);
        for (auto& c : captions) g.drawText (c.second, c.first, juce::Justification::centredLeft);
        g.setColour (col::lineSoft);
        for (auto& x : dividers) g.drawVerticalLine (x, (float) knobRow.getY() + 4, (float) knobRow.getBottom() - 4);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (styleCard.contains (e.position) && onOpenStylePicker) onOpenStylePicker (band);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 10);
        titleArea = r.removeFromTop (22).toFloat();
        r.removeFromTop (6);

        auto left = r.removeFromLeft (222);
        styleCard = left.removeFromTop (76).toFloat();
        left.removeFromTop (8);
        channels.setBounds (left.removeFromTop (26));
        left.removeFromTop (8);
        auto row = left.removeFromTop (26);
        bypass.setBounds (row.removeFromLeft (80));
        row.removeFromLeft (8);
        menuBtn.setBounds (row);

        auto right = r.removeFromRight (196);
        curve.setBounds (right.withTrimmedTop (-28));

        r.removeFromLeft (18); r.removeFromRight (18);
        knobRow = r;
        captions.clear(); dividers.clear();
        auto cap = r.removeFromTop (16);
        const int small = (r.getWidth() - 116) / 7;
        auto place = [&] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w).withTrimmedTop (8)); };
        auto caption = [&] (const juce::String& t, int w) { captions.push_back ({ cap.removeFromLeft (w).toFloat().withTrimmedLeft (4), t }); };

        caption ("CHARACTER", 116 + small * 2);
        caption ("SHAPE", small * 2);
        caption ("OUTPUT", small * 3);
        place (drive, 116);
        place (fb, small); place (fbf, small);
        dividers.push_back (r.getX());
        place (dyn, small); place (tone, small);
        dividers.push_back (r.getX());
        place (mix, small); place (lvl, small); place (width, small);
    }

    int getBand() const { return band; }

private:
    std::vector<Knob*> knobs() { return { &drive, &fb, &fbf, &dyn, &tone, &mix, &lvl, &width }; }

    AnjisAudioProcessor& proc;
    int band = 0, lastStyle = -1;
    float lastDrive = -1;
    Knob drive, fb, fbf, dyn, tone, mix, lvl, width;
    Segmented channels;
    Toggle bypass;
    juce::TextButton menuBtn;
    CurveView curve;
    juce::Rectangle<float> titleArea, styleCard;
    juce::Rectangle<int> knobRow;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> captions;
    std::vector<int> dividers;
    Anim styleHover;
};
} // namespace anjis::ui
