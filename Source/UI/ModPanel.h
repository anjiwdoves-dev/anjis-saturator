#pragma once
#include "Knob.h"

namespace anjis::ui
{
inline void drawGrip (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre ({ r.getCentreX() - 2.2f + (float) j * 4.4f, r.getCentreY() - 4.4f + (float) i * 4.4f }));
}

// A draggable modulation source pill.
class DragChip : public juce::Component, public Animated
{
public:
    DragChip (AnjisAudioProcessor& p, int source, juce::String text) : proc (p), src (source), label (std::move (text))
    {
        setHelp (*this, "Drag onto any knob to modulate it with " + sourceNames()[src] + ".");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    void animTick() override
    {
        float v = proc.srcValue[src].load();
        if (isBipolar (src)) v = 0.5f + 0.5f * v;
        if (std::abs (v - shown) > 0.002f) { shown = v; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (col::panel); g.fillRoundedRectangle (r, 8);
        g.setColour (isMouseOver() ? col::mod : col::line); g.drawRoundedRectangle (r, 8, 1);
        drawGrip (g, r.removeFromLeft (18), col::ink3);
        auto meter = r.removeFromBottom (7).reduced (4, 2).withTrimmedRight (6);
        g.setColour (col::lineSoft); g.fillRoundedRectangle (meter, 1.5f);
        g.setColour (col::purple); g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, shown)), 1.5f);
        g.setColour (col::ink); g.setFont (Fonts::strong (11.5f));
        g.drawText (label, r, juce::Justification::centredLeft);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 4)
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! c->isDragAndDropActive()) c->startDragging ("mod:" + juce::String (src), this);
    }
private:
    AnjisAudioProcessor& proc;
    int src; juce::String label; float shown = 0;
};

// --------------------------------------------------------------- LFO
class LfoView : public juce::Component, public Animated
{
public:
    LfoView (AnjisAudioProcessor& p, int index) : proc (p), l (index)
    { setHelp (*this, "LFO shape. In Steps mode, click and drag to draw the steps."); }

    void animTick() override { repaint(); }

    float shapeAt (int shape, float ph) const
    {
        switch (shape)
        {
            case 0: return std::sin (ph * 6.2831853f);
            case 1: return ph < 0.25f ? ph * 4 : ph < 0.75f ? 2 - ph * 4 : ph * 4 - 4;
            case 2: return ph * 2 - 1;
            case 3: return 1 - ph * 2;
            case 4: return ph < 0.5f ? 1.0f : -1.0f;
            case 5: { const float v[] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, -0.5f, 0.4f, -0.1f }; return v[(int) (ph * 8) % 8]; }
            case 6: { const float v[] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, -0.5f, 0.4f, -0.1f, 0.6f };
                      const float x = ph * 8; const int i = (int) x; const float f = x - (float) i; const float c = 0.5f - 0.5f * std::cos (f * 3.14159f);
                      return v[i] + (v[i + 1] - v[i]) * c; }
            default: return 0;
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 10);
        auto a = r.reduced (10, 10);
        g.setColour (col::line); g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());
        const int shape = (int) proc.get (lfoId (l, "shape"));
        const float ph = proc.lfoPhase[l].load();
        auto yFor = [&] (float v) { return a.getCentreY() - v * a.getHeight() * 0.46f; };
        if (shape == 7)
        {
            const int n = (int) proc.get (lfoId (l, "nsteps"));
            const float w = a.getWidth() / (float) n;
            const int cur = std::min ((int) (ph * (float) n), n - 1);
            for (int s = 0; s < n; ++s)
            {
                const float v = proc.get (lfoStepId (l, s));
                auto bar = juce::Rectangle<float>::leftTopRightBottom (a.getX() + (float) s * w + 1.5f, std::min (yFor (v), a.getCentreY()),
                                                                     a.getX() + (float) (s + 1) * w - 1.5f, std::max (yFor (v), a.getCentreY()));
                g.setColour (s == cur ? col::purple : col::mod.withAlpha (0.7f));
                g.fillRoundedRectangle (bar, 2.5f);
            }
        }
        else
        {
            juce::Path p;
            for (int i = 0; i <= 120; ++i)
            {
                const float x = (float) i / 120.0f;
                const float pt = std::fmod (x + proc.get (lfoId (l, "phase")) / 360.0f, 1.0f);
                const float y = yFor (shapeAt (shape, pt));
                if (i == 0) p.startNewSubPath (a.getX() + x * a.getWidth(), y); else p.lineTo (a.getX() + x * a.getWidth(), y);
            }
            g.setColour (col::purple); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // playhead
        float ph0 = ph - proc.get (lfoId (l, "phase")) / 360.0f; ph0 -= std::floor (ph0);
        const float x = a.getX() + (shape == 7 ? ph : ph0) * a.getWidth();
        g.setColour (col::purpleDk.withAlpha (0.35f)); g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        const float v = proc.srcValue[(int) Src::Lfo1 + l].load();
        g.setColour (col::purple.withAlpha (0.2f)); g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre ({ x, yFor (v) }));
        g.setColour (col::purpleDk); g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre ({ x, yFor (v) }));
    }

    void mouseDown (const juce::MouseEvent& e) override { proc.undo.beginNewTransaction(); edit (e); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }

private:
    void edit (const juce::MouseEvent& e)
    {
        if ((int) proc.get (lfoId (l, "shape")) != 7) return;
        auto a = getLocalBounds().toFloat().reduced (10, 10);
        const int n = (int) proc.get (lfoId (l, "nsteps"));
        const int s = juce::jlimit (0, n - 1, (int) ((e.position.x - a.getX()) / a.getWidth() * (float) n));
        const float v = juce::jlimit (-1.0f, 1.0f, (a.getCentreY() - e.position.y) / (a.getHeight() * 0.46f));
        proc.set (lfoStepId (l, s), std::abs (v) < 0.04f ? 0.0f : v);
    }
    AnjisAudioProcessor& proc;
    int l;
};

class LfoEditor : public juce::Component, public Animated
{
public:
    LfoEditor (AnjisAudioProcessor& p, int index)
        : proc (p), l (index), view (p, index), shape (p), div (p), rate (p, "Rate"), phase (p, "Phase"), glide (p, "Glide"),
          steps (p, "Steps"), sync (p, "Sync"), retrig (p, "Retrig"), chip (p, (int) Src::Lfo1 + index, "LFO " + juce::String (index + 1))
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &view, &shape, &div, &rate, &phase, &glide, &steps, &sync, &retrig, &chip })
            addAndMakeVisible (c);
        shape.setParam (lfoId (l, "shape")); div.setParam (lfoId (l, "div")); rate.setParam (lfoId (l, "rate"));
        phase.setParam (lfoId (l, "phase")); glide.setParam (lfoId (l, "glide")); steps.setParam (lfoId (l, "nsteps"));
        sync.setParam (lfoId (l, "sync")); retrig.setParam (lfoId (l, "retrig"));
        setHelp (sync, "Lock the LFO to your project tempo.");
        setHelp (retrig, "Restart the LFO on every MIDI note.");
        setHelp (shape, "LFO waveform. Pick Steps to draw a 16-step pattern.");
    }
    void animTick() override
    {
        const bool s = proc.get (lfoId (l, "sync")) > 0.5f;
        rate.setVisible (! s); div.setVisible (s);
        const bool st = (int) proc.get (lfoId (l, "shape")) == 7;
        steps.setVisible (st); glide.setVisible (st);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto side = r.removeFromLeft (130);
        chip.setBounds (side.removeFromTop (32));
        side.removeFromTop (8);
        shape.setBounds (side.removeFromTop (28));
        side.removeFromTop (8);
        auto row = side.removeFromTop (28);
        sync.setBounds (row.removeFromLeft (61)); row.removeFromLeft (8); retrig.setBounds (row);
        side.removeFromTop (8);
        div.setBounds (side.removeFromTop (28));
        r.removeFromLeft (14);
        auto knobs = r.removeFromRight (300);
        r.removeFromRight (14);
        view.setBounds (r);
        const int kw = knobs.getWidth() / 4;
        rate.setBounds (knobs.removeFromLeft (kw)); phase.setBounds (knobs.removeFromLeft (kw));
        steps.setBounds (knobs.removeFromLeft (kw)); glide.setBounds (knobs.removeFromLeft (kw));
        if (proc.get (lfoId (l, "sync")) > 0.5f) phase.setBounds (phase.getBounds());
    }
private:
    AnjisAudioProcessor& proc;
    int l;
    LfoView view;
    Choice shape, div;
    Knob rate, phase, glide, steps;
    Toggle sync, retrig;
    DragChip chip;
};

// --------------------------------------------------------------- Envelope
class EnvView : public juce::Component, public Animated
{
public:
    EnvView (AnjisAudioProcessor& p, int index) : proc (p), e (index) {}
    void animTick() override { repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 10);
        auto a = r.reduced (12, 12);
        auto tw = [] (float ms) { return std::log1p (ms / 20.0f); };
        const float A = tw (proc.get (egId (e, "a"))), D = tw (proc.get (egId (e, "d"))), R = tw (proc.get (egId (e, "r"))), S = 1.2f;
        const float total = A + D + R + S, sus = proc.get (egId (e, "s")) * 0.01f;
        const float c = std::exp (proc.get (egId (e, "curve")) * 0.016f);
        auto X = [&] (float t) { return a.getX() + t / total * a.getWidth(); };
        auto Y = [&] (float v) { return a.getBottom() - std::pow (v, c) * a.getHeight(); };
        juce::Path p; p.startNewSubPath (X (0), Y (0));
        for (int i = 1; i <= 20; ++i) { const float k = (float) i / 20; p.lineTo (X (A * k), Y (k)); }
        for (int i = 1; i <= 20; ++i) { const float k = (float) i / 20; p.lineTo (X (A + D * k), Y (1 + (sus - 1) * k)); }
        p.lineTo (X (A + D + S), Y (sus));
        for (int i = 1; i <= 20; ++i) { const float k = (float) i / 20; p.lineTo (X (A + D + S + R * k), Y (sus * (1 - k))); }
        juce::Path fill (p); fill.lineTo (X (total), a.getBottom()); fill.lineTo (X (0), a.getBottom()); fill.closeSubPath();
        g.setColour (col::lilac.withAlpha (0.6f)); g.fillPath (fill);
        g.setColour (col::purple); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float lv = proc.srcValue[(int) Src::Eg1 + e].load();
        g.setColour (col::purpleDk.withAlpha (0.12f));
        g.fillRect (a.withTop (a.getBottom() - lv * a.getHeight()).withWidth (6.0f).translated (a.getWidth() - 6, 0));
        g.setColour (col::purpleDk);
        g.fillRoundedRectangle (juce::Rectangle<float> (a.getRight() - 6, a.getBottom() - lv * a.getHeight(), 6, lv * a.getHeight()), 2);
    }
private:
    AnjisAudioProcessor& proc; int e;
};

class EnvEditor : public juce::Component
{
public:
    EnvEditor (AnjisAudioProcessor& p, int index)
        : view (p, index), a (p, "Attack"), d (p, "Decay"), s (p, "Sustain"), r (p, "Release"), curve (p, "Curve"),
          thr (p, "Threshold"), trig (p), chip (p, (int) Src::Eg1 + index, "Envelope " + juce::String (index + 1))
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &view, &a, &d, &s, &r, &curve, &thr, &trig, &chip }) addAndMakeVisible (c);
        a.setParam (egId (index, "a")); d.setParam (egId (index, "d")); s.setParam (egId (index, "s")); r.setParam (egId (index, "r"));
        curve.setParam (egId (index, "curve")); thr.setParam (egId (index, "thr")); trig.setParam (egId (index, "trig"));
        setHelp (trig, "Trigger the envelope from MIDI notes or from loud hits in the audio.");
        setHelp (thr, "Audio trigger level: hits louder than this fire the envelope.");
    }
    void resized() override
    {
        auto rr = getLocalBounds();
        auto side = rr.removeFromLeft (130);
        chip.setBounds (side.removeFromTop (32)); side.removeFromTop (8);
        trig.setBounds (side.removeFromTop (28)); side.removeFromTop (6);
        thr.setBounds (side.withHeight (std::min (side.getHeight(), 78)));
        rr.removeFromLeft (14);
        auto knobs = rr.removeFromRight (360); rr.removeFromRight (14);
        view.setBounds (rr);
        const int kw = knobs.getWidth() / 5;
        for (auto* k : { &a, &d, &s, &r, &curve }) k->setBounds (knobs.removeFromLeft (kw));
    }
private:
    EnvView view; Knob a, d, s, r, curve, thr; Choice trig; DragChip chip;
};

// --------------------------------------------------------------- Follower
class FollowView : public juce::Component, public Animated
{
public:
    FollowView (AnjisAudioProcessor& p, int index) : proc (p), e (index) {}
    void animTick() override { hist[(size_t) pos] = proc.srcValue[(int) Src::Ef1 + e].load(); pos = (pos + 1) % (int) hist.size(); repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 10);
        auto a = r.reduced (12, 12);
        const int n = (int) hist.size();
        juce::Path p; p.startNewSubPath (a.getX(), a.getBottom());
        for (int i = 0; i < n; ++i) p.lineTo (a.getX() + a.getWidth() * (float) i / (float) (n - 1), a.getBottom() - hist[(size_t) ((pos + i) % n)] * a.getHeight());
        p.lineTo (a.getRight(), a.getBottom()); p.closeSubPath();
        g.setColour (col::lilac); g.fillPath (p);
        g.setColour (col::purple); g.strokePath (p, juce::PathStrokeType (1.5f));
    }
private:
    AnjisAudioProcessor& proc; int e; std::array<float, 150> hist {}; int pos = 0;
};

class FollowEditor : public juce::Component
{
public:
    FollowEditor (AnjisAudioProcessor& p, int index)
        : view (p, index), att (p, "Attack"), rel (p, "Release"), gain (p, "Gain"), trans (p, "Transient"),
          chip (p, (int) Src::Ef1 + index, "Follower " + juce::String (index + 1))
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &view, &att, &rel, &gain, &trans, &chip }) addAndMakeVisible (c);
        att.setParam (efId (index, "att")); rel.setParam (efId (index, "rel")); gain.setParam (efId (index, "gain")); trans.setParam (efId (index, "trans"));
        setHelp (trans, "Transient mode reacts only to the attack of each hit, ignoring the sustain.");
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto side = r.removeFromLeft (130);
        chip.setBounds (side.removeFromTop (32)); side.removeFromTop (8);
        trans.setBounds (side.removeFromTop (28));
        r.removeFromLeft (14);
        auto knobs = r.removeFromRight (240); r.removeFromRight (14);
        view.setBounds (r);
        const int kw = knobs.getWidth() / 3;
        att.setBounds (knobs.removeFromLeft (kw)); rel.setBounds (knobs.removeFromLeft (kw)); gain.setBounds (knobs);
    }
private:
    FollowView view; Knob att, rel, gain; Toggle trans; DragChip chip;
};

// --------------------------------------------------------------- XY
class XyPad : public juce::Component, public Animated
{
public:
    XyPad (AnjisAudioProcessor& p, int index) : proc (p), i (index) { setHelp (*this, "Drag the dot. Drag the X and Y chips onto knobs to control two things at once."); }
    void animTick() override
    {
        const float x = proc.get (xyId (i, "x")), y = proc.get (xyId (i, "y"));
        if (x != lx || y != ly || isMouseButtonDown()) { lx = x; ly = y; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 10);
        auto a = r.reduced (10);
        g.setColour (col::lineSoft);
        for (int k = 1; k < 4; ++k)
        {
            g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) k / 4), a.getY(), a.getBottom());
            g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) k / 4), a.getX(), a.getRight());
        }
        const juce::Point<float> pt (a.getX() + lx * a.getWidth(), a.getBottom() - ly * a.getHeight());
        g.setColour (col::mod); g.drawVerticalLine ((int) pt.x, a.getY(), a.getBottom()); g.drawHorizontalLine ((int) pt.y, a.getX(), a.getRight());
        g.setColour (col::purple.withAlpha (0.18f)); g.fillEllipse (juce::Rectangle<float> (28, 28).withCentre (pt));
        g.setColour (col::purple); g.fillEllipse (juce::Rectangle<float> (14, 14).withCentre (pt));
        g.setColour (col::panel); g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (pt));
        g.setColour (col::ink3); g.setFont (Fonts::strong (10.5f));
        g.drawText ("XY " + juce::String (i + 1), a.reduced (4), juce::Justification::topLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        proc.undo.beginNewTransaction();
        for (auto* ax : { "x", "y" }) proc.param (xyId (i, ax))->beginChangeGesture();
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto a = getLocalBounds().toFloat().reduced (10);
        proc.param (xyId (i, "x"))->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth()));
        proc.param (xyId (i, "y"))->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight()));
    }
    void mouseUp (const juce::MouseEvent&) override { for (auto* ax : { "x", "y" }) proc.param (xyId (i, ax))->endChangeGesture(); }
private:
    AnjisAudioProcessor& proc; int i; float lx = -1, ly = -1;
};

class XyEditor : public juce::Component
{
public:
    explicit XyEditor (AnjisAudioProcessor& p)
        : pad1 (p, 0), pad2 (p, 1),
          x1 (p, (int) Src::Xy1X, juce::String::fromUTF8 ("XY 1 · X")), y1 (p, (int) Src::Xy1Y, juce::String::fromUTF8 ("XY 1 · Y")),
          x2 (p, (int) Src::Xy2X, juce::String::fromUTF8 ("XY 2 · X")), y2 (p, (int) Src::Xy2Y, juce::String::fromUTF8 ("XY 2 · Y"))
    { for (juce::Component* c : std::initializer_list<juce::Component*> { &pad1, &pad2, &x1, &y1, &x2, &y2 }) addAndMakeVisible (c); }
    void resized() override
    {
        auto r = getLocalBounds();
        auto half = [&] (XyPad& pad, DragChip& a, DragChip& b, juce::Rectangle<int> area)
        {
            auto chips = area.removeFromLeft (110);
            a.setBounds (chips.removeFromTop (32)); chips.removeFromTop (8); b.setBounds (chips.removeFromTop (32));
            area.removeFromLeft (10);
            pad.setBounds (area.withSizeKeepingCentre (std::min (area.getWidth(), area.getHeight() * 2), area.getHeight()));
        };
        half (pad1, x1, y1, r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (12));
        half (pad2, x2, y2, r.withTrimmedLeft (12));
    }
private:
    XyPad pad1, pad2; DragChip x1, y1, x2, y2;
};

// --------------------------------------------------------------- Macros
class MacroEditor : public juce::Component
{
public:
    explicit MacroEditor (AnjisAudioProcessor& p)
    {
        for (int m = 0; m < kNumMacros; ++m)
        {
            knobs.add (new Knob (p, "Macro " + juce::String (m + 1)));
            knobs.getLast()->setParam (macroId (m));
            chips.add (new DragChip (p, (int) Src::Macro1 + m, "Macro " + juce::String (m + 1)));
            addAndMakeVisible (knobs.getLast()); addAndMakeVisible (chips.getLast());
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int w = r.getWidth() / kNumMacros;
        for (int m = 0; m < kNumMacros; ++m)
        {
            auto c = r.removeFromLeft (w).reduced (16, 0);
            chips[m]->setBounds (c.removeFromBottom (30).withSizeKeepingCentre (std::min (c.getWidth(), 130), 30));
            knobs[m]->setBounds (c.reduced (0, 2));
        }
    }
private:
    juce::OwnedArray<Knob> knobs; juce::OwnedArray<DragChip> chips;
};

// --------------------------------------------------------------- MIDI
class MidiEditor : public juce::Component
{
public:
    explicit MidiEditor (AnjisAudioProcessor& p)
    {
        const char* names[] = { "Velocity", "Note", "Mod Wheel", "Pitch Bend", "Aftertouch" };
        for (int i = 0; i < 5; ++i) { chips.add (new DragChip (p, (int) Src::Velocity + i, names[i])); addAndMakeVisible (chips.getLast()); }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::ink2); g.setFont (Fonts::label (12.5f));
        g.drawText ("Send MIDI to the plugin from your DAW, then drag a source onto any knob. Right-click a knob for MIDI Learn.",
                    getLocalBounds().removeFromTop (24), juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (36).removeFromTop (34);
        const int w = r.getWidth() / 5;
        for (auto* c : chips) c->setBounds (r.removeFromLeft (w).reduced (6, 0));
    }
private:
    juce::OwnedArray<DragChip> chips;
};

// --------------------------------------------------------------- Routing list
class RoutingEditor : public juce::Component, public Animated
{
public:
    explicit RoutingEditor (AnjisAudioProcessor& p) : proc (p)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (addBtn);
        addBtn.setButtonText ("+ Add routing");
        addBtn.onClick = [this]
        {
            for (int s = 0; s < kNumSlots; ++s)
                if ((int) proc.get (slotId (s, "src")) == 0)
                {
                    proc.undo.beginNewTransaction ("Add routing");
                    proc.set (slotId (s, "src"), (float) (int) Src::Lfo1);
                    proc.set (slotId (s, "dst"), 1.0f);
                    proc.set (slotId (s, "amt"), 25.0f);
                    break;
                }
        };
        setHelp (addBtn, "Add a modulation routing. Up to 50 routings.");
    }

    int count() const
    {
        int n = 0;
        for (int s = 0; s < kNumSlots; ++s) if ((int) proc.get (slotId (s, "src")) > 0) ++n;
        return n;
    }

    void animTick() override
    {
        juce::Array<int> active;
        for (int s = 0; s < kNumSlots; ++s) if ((int) proc.get (slotId (s, "src")) > 0) active.add (s);
        if (active != shown) { shown = active; rebuild(); }
    }

    void paint (juce::Graphics& g) override
    {
        if (shown.isEmpty())
        {
            g.setColour (col::ink3); g.setFont (Fonts::label (13));
            g.drawText ("No routings yet. Drag any source chip onto a knob, or press + Add routing.",
                        getLocalBounds().withTrimmedRight (150), juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        addBtn.setBounds (r.removeFromRight (130).removeFromTop (30));
        r.removeFromRight (12);
        viewport.setBounds (r);
        layoutRows();
    }

private:
    struct Row : public juce::Component
    {
        Row (AnjisAudioProcessor& p, int s) : proc (p), slot (s), src (p), dst (p)
        {
            src.setParam (slotId (s, "src")); dst.setParam (slotId (s, "dst"));
            amt.setSliderStyle (juce::Slider::LinearBar);
            amt.setColour (juce::Slider::trackColourId, col::lilac);
            amt.setColour (juce::Slider::backgroundColourId, col::panel2);
            amt.setColour (juce::Slider::textBoxTextColourId, col::ink);
            amt.setColour (juce::Slider::textBoxOutlineColourId, col::line);
            amtAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, slotId (s, "amt"), amt);
            del.setButtonText ("Remove");
            del.onClick = [this] { proc.undo.beginNewTransaction(); proc.set (slotId (slot, "src"), 0); proc.set (slotId (slot, "dst"), 0); };
            for (juce::Component* c : std::initializer_list<juce::Component*> { &src, &dst, &amt, &del }) addAndMakeVisible (c);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (col::ink3); g.setFont (Fonts::strong (12));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x92"), arrow, juce::Justification::centred);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 3);
            src.setBounds (r.removeFromLeft (170)); arrow = r.removeFromLeft (30);
            dst.setBounds (r.removeFromLeft (220)); r.removeFromLeft (12);
            del.setBounds (r.removeFromRight (80)); r.removeFromRight (12);
            amt.setBounds (r);
        }
        AnjisAudioProcessor& proc; int slot;
        Choice src, dst; juce::Slider amt; juce::TextButton del; juce::Rectangle<int> arrow;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAtt;
    };

    void rebuild()
    {
        rows.clear();
        for (int s : shown) { rows.add (new Row (proc, s)); list.addAndMakeVisible (rows.getLast()); }
        layoutRows();
        repaint();
    }
    void layoutRows()
    {
        const int w = viewport.getWidth() - 12;
        list.setSize (std::max (w, 10), rows.size() * 36);
        for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, i * 36, w, 36);
    }

    AnjisAudioProcessor& proc;
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<Row> rows;
    juce::Array<int> shown;
    juce::TextButton addBtn;
};

// --------------------------------------------------------------- Panel with tabs
class ModPanel : public juce::Component, public Animated
{
public:
    explicit ModPanel (AnjisAudioProcessor& p) : proc (p), xy (p), macros (p), midi (p), routing (p)
    {
        for (int l = 0; l < kNumLfos; ++l) { lfos.add (new LfoEditor (p, l)); pages.push_back (lfos.getLast()); names.add ("LFO " + juce::String (l + 1)); tabSrc.push_back ((int) Src::Lfo1 + l); }
        for (int e = 0; e < kNumEgs; ++e) { envs.add (new EnvEditor (p, e)); pages.push_back (envs.getLast()); names.add ("Env " + juce::String (e + 1)); tabSrc.push_back ((int) Src::Eg1 + e); }
        for (int e = 0; e < kNumEfs; ++e) { fols.add (new FollowEditor (p, e)); pages.push_back (fols.getLast()); names.add ("Follow " + juce::String (e + 1)); tabSrc.push_back ((int) Src::Ef1 + e); }
        pages.push_back (&xy); names.add ("XY"); tabSrc.push_back (0);
        pages.push_back (&macros); names.add ("Macros"); tabSrc.push_back (0);
        pages.push_back (&midi); names.add ("MIDI"); tabSrc.push_back (0);
        pages.push_back (&routing); names.add ("Routing"); tabSrc.push_back (-1);
        for (auto* pg : pages) addChildComponent (pg);
        pages[0]->setVisible (true);
        tabAnim.snap (0);
        setHelp (*this, "Modulation: pick a source, then drag its chip (or its tab) onto any knob. Alt-drag a knob to change the amount.");
    }

    void animTick() override
    {
        tabAnim.target = (float) current;
        bool d = tabAnim.tick();
        for (size_t i = 0; i < tabSrc.size(); ++i)
            if (tabSrc[i] > 0)
            {
                float v = proc.srcValue[tabSrc[i]].load();
                if (isBipolar (tabSrc[i])) v = 0.5f + 0.5f * v;
                if (std::abs (v - pulse[i]) > 0.01f) { pulse[i] = v; d = true; }
            }
        const int rc = routing.count();
        if (rc != routeCount) { routeCount = rc; d = true; }
        if (d) repaint (tabBar);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (r, 14);

        g.setFont (Fonts::title (14)); g.setColour (col::ink);
        g.drawText ("Modulation", titleArea, juce::Justification::centredLeft);

        // sliding highlight
        const int i0 = (int) std::floor (tabAnim.value), i1 = std::min (i0 + 1, (int) tabRects.size() - 1);
        const float f = tabAnim.value - (float) i0;
        auto hl = tabRects[(size_t) i0].toFloat();
        auto h2 = tabRects[(size_t) i1].toFloat();
        hl = juce::Rectangle<float> (hl.getX() + (h2.getX() - hl.getX()) * f, hl.getY(), hl.getWidth() + (h2.getWidth() - hl.getWidth()) * f, hl.getHeight());
        g.setColour (col::lilac); g.fillRoundedRectangle (hl, 8);

        for (size_t i = 0; i < tabRects.size(); ++i)
        {
            auto t = tabRects[i].toFloat();
            const bool cur = (int) i == current;
            if (tabSrc[i] > 0)
            {
                auto dot = juce::Rectangle<float> (7, 7).withCentre ({ t.getX() + 12, t.getCentreY() });
                g.setColour (col::mod.withAlpha (0.25f + 0.75f * pulse[i]));
                g.fillEllipse (dot.expanded (pulse[i] * 1.5f));
            }
            g.setColour (cur ? col::purpleDk : col::ink2);
            g.setFont (Fonts::strong (11.5f));
            juce::String label = names[(int) i];
            if (tabSrc[i] == -1 && routeCount > 0) label << "  " << routeCount;
            g.drawText (label, t.withTrimmedLeft (tabSrc[i] > 0 ? 22 : 0), tabSrc[i] > 0 ? juce::Justification::centredLeft : juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 10);
        auto top = r.removeFromTop (28);
        titleArea = top.removeFromLeft (110).toFloat();
        tabBar = top;
        tabRects.clear();
        for (size_t i = 0; i < pages.size(); ++i)
        {
            const int w = tabSrc[i] > 0 ? (names[(int) i].length() > 5 ? 88 : 70) : (tabSrc[i] < 0 ? 84 : 66);
            tabRects.push_back (top.removeFromLeft (w).reduced (2, 0));
        }
        r.removeFromTop (12);
        for (auto* pg : pages) pg->setBounds (r);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (size_t i = 0; i < tabRects.size(); ++i)
            if (tabRects[i].contains (e.getPosition())) { select ((int) i); dragTab = (int) i; return; }
        dragTab = -1;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragTab >= 0 && tabSrc[(size_t) dragTab] > 0 && e.getDistanceFromDragStart() > 5)
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! c->isDragAndDropActive())
                {
                    juce::Image img (juce::Image::ARGB, tabRects[(size_t) dragTab].getWidth(), tabRects[(size_t) dragTab].getHeight(), true);
                    { juce::Graphics g (img); g.setColour (col::purple); g.fillRoundedRectangle (img.getBounds().toFloat(), 8);
                      g.setColour (col::panel); g.setFont (Fonts::strong (11.5f)); g.drawText (names[dragTab], img.getBounds(), juce::Justification::centred); }
                    c->startDragging ("mod:" + juce::String (tabSrc[(size_t) dragTab]), this, juce::ScaledImage (img));
                }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        bool overDraggable = false;
        for (size_t i = 0; i < tabRects.size(); ++i) if (tabRects[i].contains (e.getPosition()) && tabSrc[i] > 0) overDraggable = true;
        setMouseCursor (overDraggable ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void select (int i)
    {
        if (i == current) return;
        pages[(size_t) current]->setVisible (false);
        current = i;
        pages[(size_t) current]->setVisible (true);
        repaint();
    }
    void showRouting() { select ((int) pages.size() - 1); }

private:
    AnjisAudioProcessor& proc;
    juce::OwnedArray<LfoEditor> lfos;
    juce::OwnedArray<EnvEditor> envs;
    juce::OwnedArray<FollowEditor> fols;
    XyEditor xy; MacroEditor macros; MidiEditor midi; RoutingEditor routing;
    std::vector<juce::Component*> pages;
    std::vector<int> tabSrc;
    juce::StringArray names;
    std::vector<juce::Rectangle<int>> tabRects;
    juce::Rectangle<float> titleArea;
    juce::Rectangle<int> tabBar;
    float pulse[16] {};
    int current = 0, dragTab = -1, routeCount = -1;
    Anim tabAnim;
};
} // namespace anjis::ui
