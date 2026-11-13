#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

namespace anjis::ui
{
namespace col
{
    const juce::Colour bg        { 0xff5a34d6 };
    const juce::Colour bgDeep    { 0xff4a26bd };
    const juce::Colour onDark    { 0xffffffff };
    const juce::Colour panel     { 0xffffffff };
    const juce::Colour panel2    { 0xfff6f5f9 };
    const juce::Colour line      { 0xffdedce5 };
    const juce::Colour lineSoft  { 0xffebe9f0 };
    const juce::Colour ink       { 0xff221f29 };
    const juce::Colour ink2      { 0xff6c6878 };
    const juce::Colour ink3      { 0xffa7a3b1 };
    const juce::Colour purple    { 0xff6a3df0 };
    const juce::Colour purpleDk  { 0xff4b23c8 };
    const juce::Colour lilac     { 0xffe4dbff };
    const juce::Colour lilac2    { 0xfff3efff };
    const juce::Colour mod       { 0xffb59bff };
    const juce::Colour warn      { 0xffe2558a };
}

struct Fonts
{
    static Fonts& get() { static Fonts f; return f; }
    juce::Typeface::Ptr poppinsMed, poppinsBold, interMed, interSemi;

    static juce::Font brand (float h)  { return juce::Font (juce::FontOptions (get().poppinsBold).withHeight (h)); }
    static juce::Font title (float h)  { return juce::Font (juce::FontOptions (get().poppinsMed).withHeight (h)); }
    static juce::Font label (float h)  { return juce::Font (juce::FontOptions (get().interMed).withHeight (h)); }
    static juce::Font strong (float h) { return juce::Font (juce::FontOptions (get().interSemi).withHeight (h)); }

private:
    Fonts()
    {
        poppinsMed  = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
        poppinsBold = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
        interMed    = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        interSemi   = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
    }
};

// Simple critically-damped-ish smoother for animations.
struct Anim
{
    float value = 0, target = 0, speed = 0.18f;
    bool tick() { const float old = value; value += (target - value) * speed; if (std::abs (target - value) < 0.0005f) value = target; return old != value; }
    void snap (float v) { value = target = v; }
};

inline void setHelp (juce::Component& c, const juce::String& text) { c.getProperties().set ("help", text); }
inline void markOnDark (juce::Component& c) { c.getProperties().set ("onDark", true); }
inline bool isOnDark (const juce::Component& c) { return (bool) c.getProperties().getWithDefault ("onDark", false); }

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::lilac);
        setColour (juce::PopupMenu::highlightedTextColourId, col::purpleDk);
        setColour (juce::PopupMenu::headerTextColourId, col::ink3);
        setColour (juce::ComboBox::backgroundColourId, col::panel);
        setColour (juce::ComboBox::textColourId, col::ink);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::ink2);
        setColour (juce::TextButton::buttonColourId, col::panel);
        setColour (juce::TextButton::buttonOnColourId, col::purple);
        setColour (juce::TextButton::textColourOffId, col::ink);
        setColour (juce::TextButton::textColourOnId, col::panel);
        setColour (juce::TooltipWindow::backgroundColourId, col::ink);
        setColour (juce::TooltipWindow::textColourId, col::panel);
        setColour (juce::TextEditor::backgroundColourId, col::panel);
        setColour (juce::TextEditor::textColourId, col::ink);
        setColour (juce::TextEditor::highlightColourId, col::lilac);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::purple);
        setColour (juce::CaretComponent::caretColourId, col::purple);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::ink);
        setColour (juce::AlertWindow::outlineColourId, col::line);
        setColour (juce::ScrollBar::thumbColourId, col::line);
        setColour (juce::Label::textColourId, col::ink);
        setColour (juce::Label::textWhenEditingColourId, col::ink);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return Fonts::get().interMed; }

    juce::Font getPopupMenuFont() override { return Fonts::label (14.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::label (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Fonts::strong (12.5f); }
    juce::Font getAlertWindowMessageFont() override { return Fonts::label (14.0f); }
    juce::Font getAlertWindowTitleFont() override { return Fonts::title (17.0f); }
    juce::Font getLabelFont (juce::Label&) override { return Fonts::label (13.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::panel);
        g.setColour (col::line);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        if (isOnDark (b))
        {
            g.setColour (on ? col::onDark : col::onDark.withAlpha (down ? 0.26f : over ? 0.2f : 0.13f));
            g.fillRoundedRectangle (r, 7.0f);
            if (! on) { g.setColour (col::onDark.withAlpha (over ? 0.45f : 0.25f)); g.drawRoundedRectangle (r, 7.0f, 1.0f); }
            return;
        }
        auto fill = on ? col::purple : (down ? col::lilac : over ? col::lilac2 : col::panel);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 7.0f);
        if (! on) { g.setColour (over ? col::mod : col::line); g.drawRoundedRectangle (r, 7.0f, 1.0f); }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        if (isOnDark (b))
            g.setColour (b.getToggleState() ? col::purpleDk : col::onDark.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
        else
            g.setColour (b.getToggleState() ? col::panel : (b.isEnabled() ? col::ink : col::ink3));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        const bool dark = isOnDark (box);
        const bool over = box.isMouseOver (true);
        g.setColour (dark ? col::onDark.withAlpha (over ? 0.2f : 0.13f) : over ? col::lilac2 : col::panel);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (dark ? col::onDark.withAlpha (over ? 0.45f : 0.25f) : over ? col::mod : col::line);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
        juce::Path p;
        const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
        p.startNewSubPath (cx - 3.5f, cy - 1.5f); p.lineTo (cx, cy + 2.0f); p.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (dark ? col::onDark : col::ink2);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, isOnDark (box) ? col::onDark : col::ink);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isSeparator)
        {
            g.setColour (col::lineSoft);
            g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }
        auto r = area.reduced (4, 1);
        if (isHighlighted && isActive) { g.setColour (col::lilac); g.fillRoundedRectangle (r.toFloat(), 5.0f); }
        g.setColour (! isActive ? col::ink3 : isHighlighted ? col::purpleDk : col::ink);
        g.setFont (getPopupMenuFont());
        auto tr = r.reduced (12, 0);
        if (isTicked)
        {
            g.setColour (col::purple);
            g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre ({ (float) r.getX() + 9.0f, (float) r.getCentreY() }));
            g.setColour (isHighlighted ? col::purpleDk : col::ink);
        }
        g.drawFittedText (text, tr.withTrimmedLeft (6), juce::Justification::centredLeft, 1);
        if (shortcut.isNotEmpty()) { g.setColour (col::ink3); g.drawText (shortcut, tr, juce::Justification::centredRight); }
        if (hasSubMenu)
        {
            juce::Path p; const float x = (float) tr.getRight() - 4, y = (float) tr.getCentreY();
            p.startNewSubPath (x - 3, y - 4); p.lineTo (x, y); p.lineTo (x - 3, y + 4);
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }
        juce::ignoreUnused (icon, textColour);
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
    {
        if (isSeparator) { w = 50; h = 9; return; }
        w = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 50;
        h = juce::jmax (26, standardHeight);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.setColour (col::ink); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6);
        g.setColour (col::panel); g.setFont (Fonts::label (12.5f));
        g.drawText (text, 0, 0, w, h, juce::Justification::centred);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool) override
    {
        auto r = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
        g.setColour (over ? col::mod : col::line);
        g.fillRoundedRectangle (r.toFloat(), 2.0f);
    }
};
} // namespace anjis::ui
