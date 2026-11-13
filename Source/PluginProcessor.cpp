#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace anjis;

namespace
{
constexpr int tBand (int band, int which) { return 1 + band * kBandTargets + which; }
constexpr int tXover (int i) { return 1 + kMaxBands * kBandTargets + i; }
constexpr int tMix = 1 + kMaxBands * kBandTargets + kMaxXovers;
constexpr int tOut = tMix + 1;
constexpr int tIn  = tMix + 2;
const char* bandSuffixes[] = { "style", "drive", "fb", "fbf", "dyn", "tone", "mix", "lvl", "width", "ch", "solo", "mute", "byp" };
}

AnjisAudioProcessor::AnjisAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "ANJIS", createLayout())
{
    for (auto& c : ccMap) c = -1;
    for (auto& v : modNorm) v = 0.0f;
    for (auto& v : srcValue) v = 0.0f;
    for (auto& v : lfoPhase) v = 0.0f;
    for (int b = 0; b < kMaxBands; ++b) { bandOutRms[b] = 0; bandInPeak[b] = 0; bandOutPeak[b] = 0; }
    cacheParams();
}

void AnjisAudioProcessor::cacheParams()
{
    pBands = rp ("bands"); pPhase = rp ("phase"); pHq = rp ("hq"); pAuto = rp ("autogain");
    for (int i = 0; i < kMaxXovers; ++i) pSlope[i] = rp (slopeId (i));
    for (int b = 0; b < kMaxBands; ++b)
    {
        pStyle[b] = rp (bandId (b, "style")); pCh[b] = rp (bandId (b, "ch"));
        pSolo[b] = rp (bandId (b, "solo"));   pMute[b] = rp (bandId (b, "mute"));
        pByp[b] = rp (bandId (b, "byp"));     pWidth[b] = rp (bandId (b, "width"));
    }
    for (int s = 0; s < kNumSlots; ++s)
    {
        pSlotSrc[s] = rp (slotId (s, "src")); pSlotDst[s] = rp (slotId (s, "dst")); pSlotAmt[s] = rp (slotId (s, "amt"));
    }
    for (int t = 1; t < kNumTargets; ++t)
    {
        targetParam[t] = apvts.getParameter (targetParamId (t));
        targetRaw[t] = rp (targetParamId (t));
        jassert (targetParam[t] != nullptr);
    }
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& r = lfoRaw[l];
        r.rate = rp (lfoId (l, "rate")); r.sync = rp (lfoId (l, "sync")); r.div = rp (lfoId (l, "div"));
        r.shape = rp (lfoId (l, "shape")); r.nsteps = rp (lfoId (l, "nsteps")); r.glide = rp (lfoId (l, "glide"));
        r.phase = rp (lfoId (l, "phase")); r.retrig = rp (lfoId (l, "retrig"));
        for (int s = 0; s < kNumSteps; ++s) r.steps[s] = rp (lfoStepId (l, s));
    }
    for (int e = 0; e < kNumEgs; ++e)
    {
        auto& r = egRaw[e];
        r.a = rp (egId (e, "a")); r.d = rp (egId (e, "d")); r.s = rp (egId (e, "s")); r.r = rp (egId (e, "r"));
        r.curve = rp (egId (e, "curve")); r.trig = rp (egId (e, "trig")); r.thr = rp (egId (e, "thr"));
    }
    for (int e = 0; e < kNumEfs; ++e)
    {
        auto& r = efRaw[e];
        r.att = rp (efId (e, "att")); r.rel = rp (efId (e, "rel")); r.gain = rp (efId (e, "gain")); r.trans = rp (efId (e, "trans"));
    }
    xyRaw[0] = rp (xyId (0, "x")); xyRaw[1] = rp (xyId (0, "y")); xyRaw[2] = rp (xyId (1, "x")); xyRaw[3] = rp (xyId (1, "y"));
    for (int m = 0; m < kNumMacros; ++m) macroRaw[m] = rp (macroId (m));
}

bool AnjisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void AnjisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    maxBlock = std::max (samplesPerBlock, kChunk);
    work.setSize (2, maxBlock);
    dryBuf.setSize (2, maxBlock);
    for (auto& b : bandBuf) b.setSize (2, kChunk);
    monoTmp.assign ((size_t) kChunk, 0.0f);

    for (auto& x : xo) x.reset();
    for (auto& b : bands) b.prepare (sampleRate, kChunk);
    mod.prepare (sampleRate);
    dryDelay.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    dryDelay.reset();

    LinearPhaseSplitter::Design d;
    d.nBands = (int) pBands();
    for (int i = 0; i < kMaxXovers; ++i) { d.f[i] = get (xoverId (i)); d.s[i] = (int) pSlope[i](); }
    linear.prepare (sampleRate, kChunk, d);

    lastLatency = -1;
    updateLatency();
}

void AnjisAudioProcessor::updateLatency()
{
    const int stages = (int) pHq();
    const int phase = (int) pPhase();
    const int lat = (phase == 1 ? linear.getLatency() : 0) + juce::roundToInt (bands[0].getLatency (stages));
    if (lat != lastLatency)
    {
        lastLatency = lat;
        latency = lat;
        dryDelay.setDelay ((float) lat);
        setLatencySamples (lat);
    }
}

float AnjisAudioProcessor::targetValue (int t, const float* offset)
{
    auto* p = targetParam[t];
    const float base = p->convertTo0to1 (targetRaw[t]());
    const float norm = juce::jlimit (0.0f, 1.0f, base + offset[t]);
    modNorm[t].store (norm, std::memory_order_relaxed);
    return p->convertFrom0to1 (norm);
}

void AnjisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = std::min (buffer.getNumChannels(), 2);
    if (n == 0 || nCh == 0) return;
    if (n > work.getNumSamples()) { work.setSize (2, n, false, false, true); dryBuf.setSize (2, n, false, false, true); }

    // ---- MIDI
    auto& params = getParameters();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) mod.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) mod.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) mod.allNotesOff();
        else if (m.isPitchWheel()) mod.pitchWheel (m.getPitchWheelValue());
        else if (m.isChannelPressure()) mod.pressure (m.getChannelPressureValue());
        else if (m.isAftertouch()) mod.pressure (m.getAfterTouchValue());
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            mod.controller (cc, v);
            const int learn = learnParam.load();
            if (learn >= 0)
            {
                for (auto& c : ccMap) if (c == learn) c = -1;
                ccMap[(size_t) cc] = learn;
                learnParam = -1;
            }
            const int pi = ccMap[(size_t) cc];
            if (pi >= 0 && pi < params.size())
                params[pi]->setValueNotifyingHost ((float) v / 127.0f);
        }
    }

    // ---- transport
    Transport tp;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tp.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) tp.bpm = *b;
            if (auto q = pos->getPpqPosition()) { tp.ppq = *q; tp.hasPpq = true; }
        }

    // ---- copy into stereo work buffer
    work.copyFrom (0, 0, buffer, 0, 0, n);
    work.copyFrom (1, 0, buffer, nCh > 1 ? 1 : 0, 0, n);

    updateLatency();
    const int nb = juce::jlimit (1, kMaxBands, (int) pBands());
    const int phaseMode = (int) pPhase();
    const int stages = juce::jlimit (0, 5, (int) pHq());
    const bool autoGain = pAuto() > 0.5f;
    if (phaseMode != lastPhaseMode) { for (auto& x : xo) x.reset(); linear.reset(); lastPhaseMode = phaseMode; }

    // gather modulation settings
    LfoSettings ls[kNumLfos];
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& r = lfoRaw[l]; auto& s = ls[l];
        s.rate = r.rate(); s.sync = r.sync() > 0.5f; s.div = (int) r.div(); s.shape = (int) r.shape();
        s.nSteps = (int) r.nsteps(); s.glide = r.glide(); s.phase = r.phase(); s.retrig = r.retrig() > 0.5f;
        for (int i = 0; i < kNumSteps; ++i) s.steps[i] = r.steps[i]();
    }
    EgSettings es[kNumEgs];
    for (int e = 0; e < kNumEgs; ++e)
    {
        auto& r = egRaw[e];
        es[e] = { r.a(), r.d(), r.s(), r.r(), r.curve(), (int) r.trig(), r.thr() };
    }
    EfSettings fs[kNumEfs];
    for (int e = 0; e < kNumEfs; ++e) fs[e] = { efRaw[e].att(), efRaw[e].rel(), efRaw[e].gain(), efRaw[e].trans() > 0.5f };
    float xy[4]; for (int i = 0; i < 4; ++i) xy[i] = xyRaw[i]();
    float macros[kNumMacros]; for (int m = 0; m < kNumMacros; ++m) macros[m] = macroRaw[m]();

    bool anySolo = false;
    for (int b = 0; b < nb; ++b) anySolo |= pSolo[b]() > 0.5f;
    const int sel = juce::jlimit (0, nb - 1, selectedBand.load());

    float peakIn = 0, peakOut = 0;
    float bandRmsAcc[kMaxBands] {}, bandInPk[kMaxBands] {}, bandOutPk[kMaxBands] {};
    int chunks = 0;
    float scopeTmp[kChunk];

    for (int start = 0; start < n; start += kChunk)
    {
        const int len = std::min (kChunk, n - start);
        float* L = work.getWritePointer (0, start);
        float* R = work.getWritePointer (1, start);
        ++chunks;

        for (int i = 0; i < len; ++i)
        {
            monoTmp[(size_t) i] = 0.5f * (L[i] + R[i]);
            peakIn = std::max (peakIn, std::abs (monoTmp[(size_t) i]));
        }
        specIn.push (monoTmp.data(), len);

        // ---- modulation
        mod.tick (monoTmp.data(), len, ls, es, fs, xy, macros, tp);
        if (tp.hasPpq) tp.ppq += (double) len / currentSr * tp.bpm / 60.0;

        float offset[kNumTargets] {};
        for (int s = 0; s < kNumSlots; ++s)
        {
            const int src = (int) pSlotSrc[s](), dst = (int) pSlotDst[s]();
            if (src > 0 && dst > 0 && src < kNumSources && dst < kNumTargets)
                offset[dst] += pSlotAmt[s]() * 0.01f * mod.values[src];
        }

        // ---- dry path (delayed to match latency)
        for (int c = 0; c < 2; ++c)
        {
            float* d = dryBuf.getWritePointer (c, start);
            const float* x = c == 0 ? L : R;
            for (int i = 0; i < len; ++i) { dryDelay.pushSample (c, x[i]); d[i] = dryDelay.popSample (c); }
        }

        // ---- input gain
        const float inG = dbToGain (targetValue (tIn, offset));
        for (int i = 0; i < len; ++i)
        {
            const float g = lastIn + (inG - lastIn) * (float) (i + 1) / (float) len;
            L[i] *= g; R[i] *= g;
        }
        lastIn = inG;

        // ---- crossovers
        float fr[kMaxBands] {}; int sl[kMaxBands] {};
        for (int i = 0; i < kMaxXovers; ++i)
        {
            fr[i] = targetValue (tXover (i), offset);
            sl[i] = (int) pSlope[i]();
        }
        for (int i = 1; i < nb - 1; ++i) fr[i] = std::max (fr[i], fr[i - 1] * 1.05f);
        for (int i = 0; i < nb - 1; ++i) fr[i] = std::min (fr[i], 20000.0f);

        if (phaseMode == 0)
        {
            float outs[kMaxBands];
            for (int c = 0; c < 2; ++c)
            {
                xo[c].setup (nb, fr, sl, currentSr);
                const float* x = c == 0 ? L : R;
                for (int i = 0; i < len; ++i)
                {
                    xo[c].process (x[i], outs);
                    for (int b = 0; b < nb; ++b) bandBuf[b].setSample (c, i, outs[b]);
                }
            }
        }
        else
        {
            linear.pull();
            LinearPhaseSplitter::Design d; d.nBands = nb;
            for (int i = 0; i < kMaxXovers; ++i) { d.f[i] = fr[i]; d.s[i] = sl[i]; }
            linear.request (d);
            juce::dsp::AudioBlock<float> in (work);
            auto inSub = in.getSubBlock ((size_t) start, (size_t) len);
            juce::dsp::AudioBlock<float> blocks[kMaxBands];
            for (int b = 0; b < nb; ++b) blocks[b] = juce::dsp::AudioBlock<float> (bandBuf[b]).getSubBlock (0, (size_t) len);
            linear.process (inSub, blocks, nb);
        }

        // ---- bands
        for (int i = 0; i < len; ++i) { L[i] = 0; R[i] = 0; }
        for (int b = 0; b < kMaxBands; ++b)
        {
            BandParams p;
            p.drive = targetValue (tBand (b, 0), offset);
            p.fb    = targetValue (tBand (b, 1), offset);
            p.fbf   = targetValue (tBand (b, 2), offset);
            p.dyn   = targetValue (tBand (b, 3), offset);
            p.tone  = targetValue (tBand (b, 4), offset);
            p.mix   = targetValue (tBand (b, 5), offset);
            p.lvl   = targetValue (tBand (b, 6), offset);
            if (b >= nb) continue;

            p.width = pWidth[b]();
            p.style = (int) pStyle[b]();
            p.ch = (int) pCh[b]();
            p.bypass = pByp[b]() > 0.5f;

            const float lo = b == 0 ? 20.0f : fr[b - 1];
            const float hi = b == nb - 1 ? 20000.0f : fr[b];
            const float centre = std::sqrt (lo * hi);

            auto block = juce::dsp::AudioBlock<float> (bandBuf[b]).getSubBlock (0, (size_t) len);
            if (b == sel)
            {
                for (int i = 0; i < len; ++i) scopeTmp[i] = 0.5f * (bandBuf[b].getSample (0, i) + bandBuf[b].getSample (1, i));
                scopeIn.push (scopeTmp, len);
            }
            bands[b].process (block, p, stages, centre, autoGain);
            bandRmsAcc[b] += bands[b].outRms;
            bandInPk[b] = std::max (bandInPk[b], bands[b].inPeak);
            bandOutPk[b] = std::max (bandOutPk[b], bands[b].outPeak);

            if (b == sel)
            {
                for (int i = 0; i < len; ++i) scopeTmp[i] = 0.5f * (bandBuf[b].getSample (0, i) + bandBuf[b].getSample (1, i));
                scopeOut.push (scopeTmp, len);
            }

            const bool audible = pMute[b]() < 0.5f && (! anySolo || pSolo[b]() > 0.5f);
            if (audible)
            {
                const float* bl = bandBuf[b].getReadPointer (0);
                const float* br = bandBuf[b].getReadPointer (1);
                for (int i = 0; i < len; ++i) { L[i] += bl[i]; R[i] += br[i]; }
            }
        }

        // ---- global mix + output
        const float mix = targetValue (tMix, offset) * 0.01f;
        const float outG = dbToGain (targetValue (tOut, offset));
        const float* dl = dryBuf.getReadPointer (0, start);
        const float* dr = dryBuf.getReadPointer (1, start);
        for (int i = 0; i < len; ++i)
        {
            const float t = (float) (i + 1) / (float) len;
            const float m = lastMix + (mix - lastMix) * t;
            const float g = lastOut + (outG - lastOut) * t;
            L[i] = (dl[i] + (L[i] - dl[i]) * m) * g;
            R[i] = (dr[i] + (R[i] - dr[i]) * m) * g;
            monoTmp[(size_t) i] = 0.5f * (L[i] + R[i]);
            peakOut = std::max (peakOut, std::abs (monoTmp[(size_t) i]));
        }
        lastMix = mix; lastOut = outG;
        specOut.push (monoTmp.data(), len);
    }

    // ---- write back
    if (nCh == 1)
    {
        auto* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (work.getSample (0, i) + work.getSample (1, i));
    }
    else
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.copyFrom (1, 0, work, 1, 0, n);
    }
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    // ---- UI feeds
    for (int s = 0; s < kNumSources; ++s) srcValue[s].store (mod.values[s], std::memory_order_relaxed);
    for (int l = 0; l < kNumLfos; ++l) lfoPhase[l].store (mod.lfoPhase[l], std::memory_order_relaxed);
    for (int b = 0; b < kMaxBands; ++b)
    {
        bandOutRms[b].store (b < nb ? bandRmsAcc[b] / (float) chunks : 0.0f, std::memory_order_relaxed);
        bandInPeak[b].store (bandInPk[b], std::memory_order_relaxed);
        bandOutPeak[b].store (bandOutPk[b], std::memory_order_relaxed);
    }
    inLevel.store (peakIn); outLevel.store (peakOut);
}

// ======================================================================= state
void AnjisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto midiTree = state.getOrCreateChildWithName ("MIDIMAP", nullptr);
    midiTree.removeAllProperties (nullptr);
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[(size_t) cc] >= 0) midiTree.setProperty ("cc" + juce::String (cc), ccMap[(size_t) cc].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AnjisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return;
        for (auto& c : ccMap) c = -1;
        auto midiTree = tree.getChildWithName ("MIDIMAP");
        for (int cc = 0; cc < 128; ++cc)
            if (midiTree.hasProperty ("cc" + juce::String (cc))) ccMap[(size_t) cc] = (int) midiTree.getProperty ("cc" + juce::String (cc));
        apvts.replaceState (tree);
    }
}

juce::String AnjisAudioProcessor::getMidiMappingFor (int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[(size_t) cc] == paramIndex) return "CC " + juce::String (cc);
    return {};
}

void AnjisAudioProcessor::clearMidiMapping (int paramIndex)
{
    for (auto& c : ccMap) if (c == paramIndex) c = -1;
}

// ======================================================================= helpers
void AnjisAudioProcessor::set (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

juce::ValueTree AnjisAudioProcessor::bandToTree (int band) const
{
    juce::ValueTree t ("BAND");
    for (auto* s : bandSuffixes) t.setProperty (s, get (bandId (band, s)), nullptr);
    return t;
}

void AnjisAudioProcessor::treeToBand (const juce::ValueTree& t, int band)
{
    for (auto* s : bandSuffixes)
        if (t.hasProperty (s)) set (bandId (band, s), (float) t.getProperty (s));
}

void AnjisAudioProcessor::copyBand (int from, int to) { treeToBand (bandToTree (from), to); }

void AnjisAudioProcessor::insertBandAt (float freq)
{
    const int nb = (int) get ("bands");
    if (nb >= kMaxBands) return;
    undo.beginNewTransaction ("Add band");

    int j = 0;
    while (j < nb - 1 && get (xoverId (j)) < freq) ++j;

    // shift crossovers j.. up by one
    for (int i = nb - 2; i >= j; --i)
    {
        set (xoverId (i + 1), get (xoverId (i)));
        set (slopeId (i + 1), get (slopeId (i)));
    }
    set (xoverId (j), freq);
    // shift bands j+1.. up, duplicating band j into j+1
    for (int b = nb - 1; b >= j; --b) copyBand (b, b + 1);

    for (int s = 0; s < kNumSlots; ++s)
    {
        int dst = (int) get (slotId (s, "dst"));
        if (dst <= 0) continue;
        const int u = dst - 1;
        if (u < kMaxBands * kBandTargets)
        {
            const int band = u / kBandTargets;
            if (band > j) set (slotId (s, "dst"), (float) tBand (band + 1, u % kBandTargets));
        }
        else if (u - kMaxBands * kBandTargets < kMaxXovers)
        {
            const int x = u - kMaxBands * kBandTargets;
            if (x >= j && x + 1 < kMaxXovers) set (slotId (s, "dst"), (float) tXover (x + 1));
        }
    }
    set ("bands", (float) (nb + 1));
    selectedBand = j + 1;
}

void AnjisAudioProcessor::removeBand (int band)
{
    const int nb = (int) get ("bands");
    if (nb <= 1 || band < 0 || band >= nb) return;
    undo.beginNewTransaction ("Remove band");

    const int xr = band < nb - 1 ? band : band - 1; // crossover to remove
    for (int i = xr; i < nb - 2; ++i)
    {
        set (xoverId (i), get (xoverId (i + 1)));
        set (slopeId (i), get (slopeId (i + 1)));
    }
    for (int b = band; b < nb - 1; ++b) copyBand (b + 1, b);

    for (int s = 0; s < kNumSlots; ++s)
    {
        int dst = (int) get (slotId (s, "dst"));
        if (dst <= 0) continue;
        const int u = dst - 1;
        if (u < kMaxBands * kBandTargets)
        {
            const int bb = u / kBandTargets;
            if (bb == band) set (slotId (s, "dst"), 0.0f);
            else if (bb > band) set (slotId (s, "dst"), (float) tBand (bb - 1, u % kBandTargets));
        }
        else if (u - kMaxBands * kBandTargets < kMaxXovers)
        {
            const int x = u - kMaxBands * kBandTargets;
            if (x == xr) set (slotId (s, "dst"), 0.0f);
            else if (x > xr) set (slotId (s, "dst"), (float) tXover (x - 1));
        }
    }
    set ("bands", (float) (nb - 1));
    selectedBand = juce::jlimit (0, nb - 2, band);
}

int AnjisAudioProcessor::addModulation (int src, int target, float amount)
{
    if (src <= 0 || target <= 0) return -1;
    for (int s = 0; s < kNumSlots; ++s)
        if ((int) get (slotId (s, "src")) == src && (int) get (slotId (s, "dst")) == target)
            return s;
    for (int s = 0; s < kNumSlots; ++s)
        if ((int) get (slotId (s, "src")) == 0 || (int) get (slotId (s, "dst")) == 0)
        {
            undo.beginNewTransaction ("Add modulation");
            set (slotId (s, "src"), (float) src);
            set (slotId (s, "dst"), (float) target);
            set (slotId (s, "amt"), amount);
            return s;
        }
    return -1;
}

juce::Array<int> AnjisAudioProcessor::slotsForTarget (int target) const
{
    juce::Array<int> r;
    if (target <= 0) return r;
    for (int s = 0; s < kNumSlots; ++s)
        if ((int) get (slotId (s, "dst")) == target && (int) get (slotId (s, "src")) > 0) r.add (s);
    return r;
}

juce::AudioProcessorEditor* AnjisAudioProcessor::createEditor() { return new AnjisEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AnjisAudioProcessor(); }
