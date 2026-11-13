#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

namespace anjis::ui
{
// Anything that wants a per-frame tick from the editor.
struct Animated { virtual ~Animated() = default; virtual void animTick() = 0; };

inline juce::Path arc (juce::Point<float> c, float r, float a0, float a1)
{
    juce::Path p; p.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true); return p;
}

class Knob : public juce::Slider, public juce::DragAndDropTarget, public Animated
{
public:
    static constexpr float kStart = -2.356f, kEnd = 2.356f;

    Knob (AnjisAudioProcessor& p, const juce::String& lbl, bool isBig = false)
        : proc (p), label (lbl), big (isBig)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity (big ? 260 : 200);
        setVelocityBasedMode (false);
        setPopupDisplayEnabled (false, false, nullptr);
    }

    void setParam (const juce::String& id)
    {
        if (id == paramId) return;
        att.reset();
        paramId = id;
        param = proc.param (id);
        target = targetForParamId (id);
        bipolar = id.endsWith ("_dyn") || id.endsWith ("_tone") || id.endsWith ("_amt") || id == "in" || id == "out"
                  || id.endsWith ("_curve") || id.endsWith ("_gain");
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *this);
        if (param != nullptr)
            setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        repaint();
    }

    void setLabel (const juce::String& l) { label = l; repaint(); }
    const juce::String& getParamId() const { return paramId; }

    void animTick() override
    {
        bool dirty = false;
        hover.target = (isMouseOverOrDragging() ? 1.0f : 0.0f);
        showValue.target = (isMouseButtonDown() || isMouseOver()) ? 1.0f : 0.0f;
        dirty |= hover.tick(); dirty |= showValue.tick(); dirty |= dropHi.tick();
        if (target > 0)
        {
            const float mn = proc.modNorm[target].load();
            const bool hasMods = ! proc.slotsForTarget (target).isEmpty();
            if (hasMods != hadMods || (hasMods && std::abs (mn - lastMod) > 0.0005f)) dirty = true;
            hadMods = hasMods; lastMod = mn;
        }
        if (dirty) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (param == nullptr) return;
        auto b = getLocalBounds().toFloat();
        const float textH = big ? 20.0f : 17.0f;
        auto knobArea = b.withTrimmedBottom (textH);
        const float d = std::min (knobArea.getWidth(), knobArea.getHeight()) - 10.0f;
        const auto c = knobArea.getCentre();
        const float r = d * 0.5f;
        const float trackW = big ? 5.0f : 3.5f;
        const float norm = (float) valueToProportionOfLength (getValue());
        const auto angle = [] (float n) { return kStart + (kEnd - kStart) * n; };

        const bool dark = isOnDark (*this);
        const juce::Colour valueCol = dark ? col::onDark : col::purple;

        // track
        g.setColour (dark ? col::onDark.withAlpha (0.22f + 0.1f * hover.value) : col::lineSoft.interpolatedWith (col::line, hover.value));
        g.strokePath (arc (c, r, kStart, kEnd), juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc
        const float from = bipolar ? 0.5f : 0.0f;
        if (std::abs (norm - from) > 0.002f)
        {
            g.setColour (valueCol);
            g.strokePath (arc (c, r, angle (std::min (from, norm)), angle (std::max (from, norm))),
                          juce::PathStrokeType (trackW + hover.value * 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // modulation ring
        if (target > 0)
        {
            auto slots = proc.slotsForTarget (target);
            if (! slots.isEmpty())
            {
                const float mr = r + (big ? 8.0f : 6.0f);
                float lo = norm, hi = norm;
                for (int s : slots)
                {
                    const float amt = proc.get (slotId (s, "amt")) * 0.01f;
                    const int src = (int) proc.get (slotId (s, "src"));
                    const float a = norm + amt, b2 = isBipolar (src) ? norm - amt : norm;
                    lo = std::min ({ lo, a, b2 }); hi = std::max ({ hi, a, b2 });
                }
                lo = juce::jlimit (0.0f, 1.0f, lo); hi = juce::jlimit (0.0f, 1.0f, hi);
                g.setColour (col::lilac);
                g.strokePath (arc (c, mr, kStart, kEnd), juce::PathStrokeType (2.0f));
                g.setColour (col::mod);
                if (hi - lo > 0.002f)
                    g.strokePath (arc (c, mr, angle (lo), angle (hi)), juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const float mn = proc.modNorm[target].load();
                const auto pt = c.getPointOnCircumference (mr, angle (mn));
                g.setColour (col::purpleDk);
                g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre (pt));
            }
        }

        // disc
        const float dr = r - (big ? 10.0f : 7.5f) + hover.value * 0.8f;
        auto disc = juce::Rectangle<float> (dr * 2, dr * 2).withCentre (c);
        g.setColour (juce::Colour (0x10000000));
        g.fillEllipse (disc.translated (0, 1.5f));
        g.setColour (col::panel);
        g.fillEllipse (disc);
        g.setColour (dark ? col::onDark : col::line.interpolatedWith (col::mod, hover.value * 0.6f));
        g.drawEllipse (disc, 1.0f);
        const auto p0 = c.getPointOnCircumference (dr * 0.35f, angle (norm));
        const auto p1 = c.getPointOnCircumference (dr * 0.78f, angle (norm));
        g.setColour (col::ink);
        g.drawLine ({ p0, p1 }, big ? 2.6f : 2.0f);

        if (dropHi.value > 0.01f)
        {
            g.setColour (col::purple.withAlpha (dropHi.value));
            juce::Path ring; ring.addEllipse (juce::Rectangle<float> ((r + 9) * 2, (r + 9) * 2).withCentre (c));
            juce::Path dashed; const float dash[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, ring, dash, 2);
            g.fillPath (dashed);
        }

        // label / value
        auto tr = b.removeFromBottom (textH);
        const float sv = showValue.value;
        g.setFont (big ? Fonts::strong (13.0f) : Fonts::label (11.5f));
        g.setColour ((dark ? col::onDark.withAlpha (0.8f) : col::ink2).withMultipliedAlpha (1.0f - sv));
        g.drawText (label, tr.translated (0, -sv * 4.0f), juce::Justification::centred);
        g.setColour ((dark ? col::onDark : col::purpleDk).withAlpha (sv));
        g.drawText (param->getCurrentValueAsText(), tr.translated (0, (1.0f - sv) * 4.0f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        proc.undo.beginNewTransaction();
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        if (e.mods.isAltDown() && target > 0 && ! proc.slotsForTarget (target).isEmpty())
        {
            altSlot = proc.slotsForTarget (target).getLast();
            altStart = proc.get (slotId (altSlot, "amt"));
            return;
        }
        altSlot = -1;
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (altSlot >= 0)
        {
            const float delta = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) * 0.5f;
            proc.set (slotId (altSlot, "amt"), juce::jlimit (-100.0f, 100.0f, altStart + delta));
            repaint();
            return;
        }
        if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (altSlot >= 0) { altSlot = -1; return; }
        if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e);
    }

    // drag-and-drop from modulation sources
    bool isInterestedInDragSource (const SourceDetails& d) override { return target > 0 && d.description.toString().startsWith ("mod:"); }
    void itemDragEnter (const SourceDetails&) override { dropHi.target = 1.0f; }
    void itemDragExit (const SourceDetails&) override { dropHi.target = 0.0f; }
    void itemDropped (const SourceDetails& d) override
    {
        dropHi.target = 0.0f;
        const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
        proc.addModulation (src, target, 50.0f);
        repaint();
    }

private:
    void showMenu()
    {
        juce::PopupMenu m;
        const int idx = param->getParameterIndex();
        m.addSectionHeader (param->getName (64));
        m.addItem (1, "Reset to default");
        m.addSeparator();
        const auto cc = proc.getMidiMappingFor (idx);
        if (cc.isNotEmpty()) m.addItem (3, "Forget MIDI " + cc);
        m.addItem (2, proc.learnParam.load() == idx ? "Waiting for MIDI CC..." : "MIDI Learn", true, proc.learnParam.load() == idx);
        auto slots = proc.slotsForTarget (target);
        if (! slots.isEmpty())
        {
            m.addSeparator();
            m.addSectionHeader ("Modulation (Alt-drag knob to adjust)");
            for (int s : slots)
            {
                const auto srcName = sourceNames()[(int) proc.get (slotId (s, "src"))];
                const float amt = proc.get (slotId (s, "amt"));
                m.addItem (100 + s, "Remove " + srcName + "  (" + (amt > 0 ? "+" : "") + juce::String (juce::roundToInt (amt)) + "%)");
            }
        }
        juce::Component::SafePointer<Knob> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, idx] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if (r == 1) safe->param->setValueNotifyingHost (safe->param->getDefaultValue());
            if (r == 2) pr.learnParam = (pr.learnParam.load() == idx ? -1 : idx);
            if (r == 3) pr.clearMidiMapping (idx);
            if (r >= 100) { pr.undo.beginNewTransaction(); pr.set (slotId (r - 100, "src"), 0); pr.set (slotId (r - 100, "dst"), 0); }
            safe->repaint();
        });
    }

    AnjisAudioProcessor& proc;
    juce::String paramId, label;
    juce::RangedAudioParameter* param = nullptr;
    int target = 0;
    bool big = false, bipolar = false, hadMods = false;
    float lastMod = -1;
    int altSlot = -1; float altStart = 0;
    Anim hover, showValue, dropHi;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Pill-shaped toggle bound to a bool parameter.
class Toggle : public juce::TextButton
{
public:
    Toggle (AnjisAudioProcessor& p, const juce::String& text) : proc (p) { setButtonText (text); setClickingTogglesState (true); }
    void setParam (const juce::String& id)
    {
        att.reset();
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *this);
    }
private:
    AnjisAudioProcessor& proc;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Combo bound to a choice parameter.
class Choice : public juce::ComboBox
{
public:
    explicit Choice (AnjisAudioProcessor& p) : proc (p) {}
    void setParam (const juce::String& id)
    {
        att.reset();
        clear (juce::dontSendNotification);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.param (id)))
            addItemList (c->choices, 1);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *this);
    }
private:
    AnjisAudioProcessor& proc;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Segmented control for a choice parameter (e.g. Stereo / Mid / Side).
class Segmented : public juce::Component, public Animated, private juce::AudioProcessorValueTreeState::Listener
{
public:
    Segmented (AnjisAudioProcessor& p, juce::StringArray opts) : proc (p), options (std::move (opts)) {}
    ~Segmented() override { if (paramId.isNotEmpty()) proc.apvts.removeParameterListener (paramId, this); }

    void setParam (const juce::String& id)
    {
        if (paramId.isNotEmpty()) proc.apvts.removeParameterListener (paramId, this);
        paramId = id;
        proc.apvts.addParameterListener (id, this);
        sel.snap ((float) (int) proc.get (id));
        repaint();
    }

    void animTick() override
    {
        sel.target = (float) (int) proc.get (paramId);
        if (sel.tick()) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 8.0f, 1.0f);
        const float w = r.getWidth() / (float) options.size();
        auto pill = juce::Rectangle<float> (r.getX() + sel.value * w, r.getY(), w, r.getHeight()).reduced (2.5f);
        g.setColour (col::purple); g.fillRoundedRectangle (pill, 6.0f);
        g.setFont (Fonts::strong (11.5f));
        for (int i = 0; i < options.size(); ++i)
        {
            const float a = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (sel.value - (float) i));
            g.setColour (col::ink2.interpolatedWith (col::panel, a));
            g.drawText (options[i], juce::Rectangle<float> (r.getX() + (float) i * w, r.getY(), w, r.getHeight()), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, options.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) options.size())));
        proc.undo.beginNewTransaction();
        proc.set (paramId, (float) i);
    }

private:
    void parameterChanged (const juce::String&, float) override {}
    AnjisAudioProcessor& proc;
    juce::StringArray options;
    juce::String paramId;
    Anim sel;
};
} // namespace anjis::ui
