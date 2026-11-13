#include "PluginEditor.h"

using namespace anjis;
using namespace anjis::ui;

AnjisEditor::AnjisEditor (AnjisAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p), presets (p),
      display (p), bandPanel (p), modPanel (p), picker (p),
      inKnob (p, "Input"), mixKnob (p, "Mix"), outKnob (p, "Output"),
      quality (p), phase (p), autoGain (p, "Auto Gain")
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addMouseListener (this, true);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &display, &bandPanel, &modPanel, &prevBtn, &nextBtn, &presetBtn, &saveBtn, &undoBtn, &redoBtn,
             &aBtn, &bBtn, &copyAbBtn, &sizeBtn, &inKnob, &mixKnob, &outKnob, &quality, &phase, &autoGain })
        content.addAndMakeVisible (c);
    content.addChildComponent (scrim);
    content.addChildComponent (picker);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &prevBtn, &nextBtn, &presetBtn, &saveBtn, &undoBtn, &redoBtn, &aBtn, &bBtn, &copyAbBtn, &sizeBtn,
             &inKnob, &mixKnob, &outKnob, &quality, &phase, &autoGain })
        markOnDark (*c);

    inKnob.setParam ("in"); mixKnob.setParam ("mix"); outKnob.setParam ("out");
    quality.setParam ("hq"); phase.setParam ("phase"); autoGain.setParam ("autogain");

    prevBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x80\xb9"));
    nextBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x80\xba"));
    saveBtn.setButtonText ("Save");
    undoBtn.setButtonText ("Undo");
    redoBtn.setButtonText ("Redo");
    aBtn.setButtonText ("A"); bBtn.setButtonText ("B");
    copyAbBtn.setButtonText ("Copy");
    sizeBtn.setButtonText ("Size");
    aBtn.setToggleState (proc.abSlot == 0, juce::dontSendNotification);
    bBtn.setToggleState (proc.abSlot == 1, juce::dontSendNotification);

    prevBtn.onClick = [this] { presets.step (-1); };
    nextBtn.onClick = [this] { presets.step (1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    aBtn.onClick = [this] { switchAB (0); };
    bBtn.onClick = [this] { switchAB (1); };
    copyAbBtn.onClick = [this]
    {
        proc.abStore[1 - proc.abSlot] = proc.apvts.copyState();
        helpText = juce::String ("Copied ") + (proc.abSlot == 0 ? "A to B" : "B to A");
    };
    sizeBtn.onClick = [this] { showSizeMenu(); };

    setHelp (prevBtn, "Previous preset"); setHelp (nextBtn, "Next preset");
    setHelp (presetBtn, "Browse presets");
    setHelp (saveBtn, "Save the current sound as your own preset");
    setHelp (undoBtn, "Undo the last change"); setHelp (redoBtn, "Redo");
    setHelp (aBtn, "A/B compare: switch to setting A"); setHelp (bBtn, "A/B compare: switch to setting B");
    setHelp (copyAbBtn, "Copy the current setting into the other A/B slot");
    setHelp (sizeBtn, "Change the window size (you can also drag the corner)");
    setHelp (inKnob, "Input gain into the whole plugin. Push it to saturate everything harder.");
    setHelp (mixKnob, "Global dry/wet mix for parallel saturation.");
    setHelp (outKnob, "Final output level.");
    setHelp (quality, "Oversampling: higher means less harsh aliasing but more CPU. 2x is a good default. 16x and 32x are very heavy, so save them for bouncing.");
    setHelp (phase, "Zero Latency uses analog-style crossovers. Linear Phase keeps phase perfect between bands but adds latency.");
    setHelp (autoGain, "Keeps each band's loudness steady while you change drive, so you hear tone, not volume.");

    display.onBandSelected = [this] { bandPanel.setBand (proc.selectedBand.load()); };
    bandPanel.onOpenStylePicker = [this] (int b)
    {
        scrim.setVisible (true); scrim.toFront (false);
        picker.open (b);
    };
    bandPanel.onBandMenu = [this] (int b) { showBandMenu (b); };
    picker.onClose = [this] { scrim.setVisible (false); };
    scrim.onClick = [this] { picker.close(); };

    const float savedScale = (float) proc.apvts.state.getProperty ("uiScale", 1.0f);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);
    setResizeLimits ((int) (kW * 0.6f), (int) (kH * 0.6f), (int) (kW * 2.0f), (int) (kH * 2.0f));
    setSize ((int) (kW * savedScale), (int) (kH * savedScale));

    vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { tick(); });
}

AnjisEditor::~AnjisEditor()
{
    vblank.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AnjisEditor::resized()
{
    scale = (float) getWidth() / (float) kW;
    content.setBounds (0, 0, kW, kH);
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
    layout();
}

void AnjisEditor::layout()
{
    auto r = juce::Rectangle<int> (0, 0, kW, kH).reduced (16, 0);
    auto header = r.removeFromTop (68).reduced (0, 12);

    header.removeFromLeft (190); // brand
    auto pr = header.removeFromLeft (360);
    prevBtn.setBounds (pr.removeFromLeft (30));
    pr.removeFromLeft (6);
    nextBtn.setBounds (pr.removeFromRight (30));
    pr.removeFromRight (6);
    saveBtn.setBounds (pr.removeFromRight (56));
    pr.removeFromRight (6);
    presetBtn.setBounds (pr);

    header.removeFromLeft (16);
    undoBtn.setBounds (header.removeFromLeft (54)); header.removeFromLeft (6);
    redoBtn.setBounds (header.removeFromLeft (54)); header.removeFromLeft (16);
    aBtn.setBounds (header.removeFromLeft (30)); header.removeFromLeft (4);
    bBtn.setBounds (header.removeFromLeft (30)); header.removeFromLeft (6);
    copyAbBtn.setBounds (header.removeFromLeft (52));

    auto knobs = header.removeFromRight (196).withTrimmedTop (-10).withTrimmedBottom (-12);
    sizeBtn.setBounds (header.removeFromRight (52).withTrimmedRight (12));
    outKnob.setBounds (knobs.removeFromRight (64));
    mixKnob.setBounds (knobs.removeFromRight (64));
    inKnob.setBounds (knobs.removeFromRight (64));

    display.setBounds (r.removeFromTop (262));
    r.removeFromTop (12);
    bandPanel.setBounds (r.removeFromTop (198));
    r.removeFromTop (12);
    modPanel.setBounds (r.removeFromTop (196));

    auto footer = r.reduced (0, 10);
    auto ctrl = footer.removeFromRight (420);
    autoGain.setBounds (ctrl.removeFromRight (96));
    ctrl.removeFromRight (8);
    phase.setBounds (ctrl.removeFromRight (130));
    ctrl.removeFromRight (8);
    quality.setBounds (ctrl.removeFromRight (82));
    latencyArea = ctrl.withTrimmedRight (10);
    helpArea = footer;

    picker.setBounds (bandPanel.getBounds().withHeight (292));
    scrim.setBounds (0, 0, kW, kH);
}

void AnjisEditor::paintContent (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (col::bg, 0, 0, col::bgDeep, 0, (float) kH, false));
    g.fillAll();

    // brand
    auto brand = juce::Rectangle<float> (18, 10, 180, 48);
    g.setColour (col::onDark);
    g.setFont (Fonts::brand (27));
    g.drawText ("anji's", brand.removeFromTop (34), juce::Justification::bottomLeft);
    g.setColour (col::onDark.withAlpha (0.7f));
    g.setFont (Fonts::strong (11));
    g.drawText ("SATURATOR", brand.withTrimmedLeft (2), juce::Justification::topLeft);

    // little animated mark next to the logo: a wave that bends with the output level
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.outLevel.load() * 1.6f);
        juce::Path w;
        const float x0 = 120, y0 = 32, wd = 46;
        for (int i = 0; i <= 40; ++i)
        {
            const float t = (float) i / 40.0f;
            const float s = std::sin (t * 6.2831853f + phaseAnim);
            const float y = ftanh (s * (1.0f + lvl * 4.0f)) / ftanh (1.0f + lvl * 4.0f);
            if (i == 0) w.startNewSubPath (x0 + t * wd, y0 - y * 8); else w.lineTo (x0 + t * wd, y0 - y * 8);
        }
        g.setColour (col::onDark.withAlpha (0.75f));
        g.strokePath (w, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }


    // footer
    g.setColour (col::onDark.withAlpha (proc.learnParam.load() >= 0 ? 1.0f : 0.85f));
    g.setFont (Fonts::label (12.5f));
    g.drawFittedText (helpText, helpArea, juce::Justification::centredLeft, 1);
    g.setColour (col::onDark.withAlpha (0.6f));
    g.setFont (Fonts::label (11.5f));
    const int lat = proc.latency.load();
    g.drawText (lat > 0 ? "Latency " + juce::String (lat) + " samples" : "Zero latency", latencyArea, juce::Justification::centredRight);
}

void AnjisEditor::tickChildren (juce::Component& c)
{
    for (auto* child : c.getChildren())
    {
        if (! child->isVisible()) continue;
        if (auto* a = dynamic_cast<Animated*> (child)) a->animTick();
        tickChildren (*child);
    }
}

void AnjisEditor::tick()
{
    tickChildren (content);
    phaseAnim += 0.035f;
    if (phaseAnim > 6.2831853f) phaseAnim -= 6.2831853f;

    const int nb = (int) proc.get ("bands");
    if (proc.selectedBand.load() >= nb) proc.selectedBand = nb - 1;
    if (bandPanel.getBand() != proc.selectedBand.load()) bandPanel.setBand (proc.selectedBand.load());

    if (picker.isVisible()) scrim.alpha = 1.0f;
    scrim.repaint();

    presetBtn.setButtonText (presets.getCurrentName());
    aBtn.setToggleState (proc.abSlot == 0, juce::dontSendNotification);
    bBtn.setToggleState (proc.abSlot == 1, juce::dontSendNotification);
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());

    updateHelp();
    content.repaint (0, 0, 200, 68);
    if (lastLatency != proc.latency.load() || helpText != lastHelp)
    {
        lastLatency = proc.latency.load(); lastHelp = helpText;
        content.repaint (helpArea.getUnion (latencyArea));
    }
}

void AnjisEditor::updateHelp()
{
    if (proc.learnParam.load() >= 0)
    {
        auto& params = proc.getParameters();
        const int i = proc.learnParam.load();
        helpText = "MIDI Learn: move a knob or fader on your controller to link it to " + (i < params.size() ? params[i]->getName (40) : juce::String()) + ".";
        return;
    }
    if (isDragAndDropActive()) { helpText = "Drop onto any knob to modulate it."; return; }
    auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    while (c != nullptr && c != this)
    {
        if (c->getProperties().contains ("help")) { helpText = c->getProperties()["help"].toString(); return; }
        c = c->getParentComponent();
    }
    if (c == nullptr) return;
    helpText = "Tip: drag a modulation tab onto any knob. Right-click knobs for MIDI Learn.";
}

void AnjisEditor::mouseDown (const juce::MouseEvent&)
{
    proc.undo.beginNewTransaction();
}

void AnjisEditor::switchAB (int slot)
{
    if (slot == proc.abSlot) return;
    proc.abStore[proc.abSlot] = proc.apvts.copyState();
    if (proc.abStore[slot].isValid()) proc.apvts.replaceState (proc.abStore[slot].createCopy());
    proc.abSlot = slot;
    bandPanel.setBand (juce::jmin (proc.selectedBand.load(), (int) proc.get ("bands") - 1));
}

void AnjisEditor::showPresetMenu()
{
    juce::PopupMenu m;
    auto list = presets.list();
    juce::StringArray cats;
    for (auto& e : list) if (! cats.contains (e.category)) cats.add (e.category);
    for (auto& cat : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < list.size(); ++i)
            if (list[i].category == cat) sub.addItem (i + 1, list[i].name, true, i == presets.currentIndex);
        m.addSubMenu (cat, sub);
    }
    m.addSeparator();
    m.addItem (1000, "Save preset...");
    m.addItem (1001, "Open presets folder");
    m.addItem (1002, "Reset to init");
    juce::Component::SafePointer<AnjisEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe, list] (int r)
    {
        if (! safe || r == 0) return;
        if (r == 1000) { safe->savePresetDialog(); return; }
        if (r == 1001) { PresetManager::userDir().startAsProcess(); return; }
        if (r == 1002) { safe->presets.loadIndex (0); return; }
        safe->presets.loadIndex (r - 1);
        safe->bandPanel.setBand (0);
    });
}

void AnjisEditor::savePresetDialog()
{
    alert = std::make_unique<juce::AlertWindow> ("Save preset", "Give your sound a name.", juce::MessageBoxIconType::NoIcon, this);
    alert->addTextEditor ("name", presets.getCurrentName() == "Init" ? "My Sound" : presets.getCurrentName());
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<AnjisEditor> safe (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int r)
    {
        if (! safe) return;
        if (r == 1) safe->presets.saveUser (safe->alert->getTextEditorContents ("name"));
        safe->alert.reset();
    }), false);
}

void AnjisEditor::showBandMenu (int band)
{
    juce::PopupMenu m, load;
    auto files = presets.bandPresets();
    for (int i = 0; i < files.size(); ++i) load.addItem (100 + i, files[i].getFileNameWithoutExtension());
    m.addSectionHeader ("Band " + juce::String (band + 1));
    m.addItem (1, "Copy band settings");
    m.addItem (2, "Paste band settings", bandClipboard.isValid());
    m.addItem (3, "Reset band");
    m.addSeparator();
    m.addItem (4, "Save band preset...");
    m.addSubMenu ("Load band preset", load, ! files.isEmpty());
    m.addSeparator();
    m.addItem (5, "Remove band", (int) proc.get ("bands") > 1);
    juce::Component::SafePointer<AnjisEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, band, files] (int r)
    {
        if (! safe || r == 0) return;
        auto& p = safe->proc;
        p.undo.beginNewTransaction();
        if (r == 1) safe->bandClipboard = p.bandToTree (band);
        if (r == 2) p.treeToBand (safe->bandClipboard, band);
        if (r == 3)
            for (auto* s : { "style", "drive", "fb", "fbf", "dyn", "tone", "mix", "lvl", "width", "ch", "solo", "mute", "byp" })
                if (auto* prm = p.param (bandId (band, s))) prm->setValueNotifyingHost (prm->getDefaultValue());
        if (r == 5) p.removeBand (band);
        if (r >= 100) safe->presets.loadBandPreset (files[r - 100], band);
        if (r == 4)
        {
            safe->alert = std::make_unique<juce::AlertWindow> ("Save band preset", "Name this band setting.", juce::MessageBoxIconType::NoIcon, safe.getComponent());
            safe->alert->addTextEditor ("name", styleNames()[(int) p.get (bandId (band, "style"))]);
            safe->alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            safe->alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            safe->alert->enterModalState (true, juce::ModalCallbackFunction::create ([safe, band] (int res)
            {
                if (! safe) return;
                if (res == 1) safe->presets.saveBandPreset (band, safe->alert->getTextEditorContents ("name"));
                safe->alert.reset();
            }), false);
        }
    });
}

void AnjisEditor::showSizeMenu()
{
    juce::PopupMenu m;
    const float sizes[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f };
    for (int i = 0; i < 6; ++i) m.addItem (i + 1, juce::String (juce::roundToInt (sizes[i] * 100)) + "%", true, std::abs (scale - sizes[i]) < 0.02f);
    juce::Component::SafePointer<AnjisEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sizeBtn), [safe, sizes] (int r)
    {
        if (safe && r > 0) safe->setSize ((int) (kW * sizes[r - 1]), (int) (kH * sizes[r - 1]));
    });
}
