#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "DSP/Crossover.h"
#include "DSP/Band.h"
#include "DSP/LinearPhase.h"
#include "DSP/Modulation.h"

// Lock-free-ish ring buffer for visual feeds (benign races are fine for drawing).
struct VisualRing
{
    static constexpr int kSize = 8192;
    std::array<float, kSize> data {};
    std::atomic<int> write { 0 };
    void push (const float* x, int n)
    {
        int w = write.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) { data[(size_t) w] = x[i]; w = (w + 1) & (kSize - 1); }
        write.store (w, std::memory_order_release);
    }
    void read (float* dest, int n) const
    {
        int w = write.load (std::memory_order_acquire);
        int r = (w - n + kSize) & (kSize - 1);
        for (int i = 0; i < n; ++i) { dest[i] = data[(size_t) r]; r = (r + 1) & (kSize - 1); }
    }
};

class AnjisAudioProcessor : public juce::AudioProcessor
{
public:
    AnjisAudioProcessor();
    ~AnjisAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Anjis Saturator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the UI
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> modNorm[anjis::kNumTargets];      // modulated normalised value per target
    std::atomic<float> srcValue[anjis::kNumSources];
    std::atomic<float> lfoPhase[anjis::kNumLfos];
    std::atomic<float> bandOutRms[anjis::kMaxBands], bandInPeak[anjis::kMaxBands], bandOutPeak[anjis::kMaxBands];
    std::atomic<float> inLevel { 0 }, outLevel { 0 };
    std::atomic<int> selectedBand { 0 };
    std::atomic<int> latency { 0 };
    VisualRing specIn, specOut, scopeIn, scopeOut;
    double getCurrentSampleRate() const { return currentSr; }

    // A/B compare
    juce::ValueTree abStore[2];
    int abSlot = 0;

    // MIDI learn
    std::atomic<int> learnParam { -1 };
    std::array<std::atomic<int>, 128> ccMap;
    juce::String getMidiMappingFor (int paramIndex) const;
    void clearMidiMapping (int paramIndex);

    // Band management helpers (message thread)
    void insertBandAt (float freq);
    void removeBand (int band);
    void copyBand (int from, int to);
    juce::ValueTree bandToTree (int band) const;
    void treeToBand (const juce::ValueTree& t, int band);

    // Modulation helpers (message thread)
    int addModulation (int src, int target, float amount);
    juce::Array<int> slotsForTarget (int target) const;

    juce::RangedAudioParameter* param (const juce::String& id) const { return apvts.getParameter (id); }
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void set (const juce::String& id, float realValue);

private:
    struct Raw
    {
        std::atomic<float>* p = nullptr;
        float operator()() const { return p->load (std::memory_order_relaxed); }
    };
    Raw rp (const juce::String& id) { return { apvts.getRawParameterValue (id) }; }

    void cacheParams();
    float targetValue (int target, const float* offset);
    void updateLatency();

    double currentSr = 44100.0;
    int maxBlock = 512;
    static constexpr int kChunk = 64;

    anjis::MonoCrossover xo[2];
    anjis::BandProcessor bands[anjis::kMaxBands];
    anjis::LinearPhaseSplitter linear;
    anjis::ModEngine mod;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 17 };

    juce::AudioBuffer<float> work, dryBuf;
    juce::AudioBuffer<float> bandBuf[anjis::kMaxBands];
    std::vector<float> monoTmp;

    // cached params
    Raw pBands, pPhase, pHq, pAuto;
    Raw pSlope[anjis::kMaxXovers];
    Raw pStyle[anjis::kMaxBands], pCh[anjis::kMaxBands], pSolo[anjis::kMaxBands], pMute[anjis::kMaxBands], pByp[anjis::kMaxBands], pWidth[anjis::kMaxBands];
    Raw pSlotSrc[anjis::kNumSlots], pSlotDst[anjis::kNumSlots], pSlotAmt[anjis::kNumSlots];
    juce::RangedAudioParameter* targetParam[anjis::kNumTargets] {};
    Raw targetRaw[anjis::kNumTargets];

    struct LfoRaw { Raw rate, sync, div, shape, nsteps, glide, phase, retrig; Raw steps[anjis::kNumSteps]; } lfoRaw[anjis::kNumLfos];
    struct EgRaw  { Raw a, d, s, r, curve, trig, thr; } egRaw[anjis::kNumEgs];
    struct EfRaw  { Raw att, rel, gain, trans; } efRaw[anjis::kNumEfs];
    Raw xyRaw[4], macroRaw[anjis::kNumMacros];

    float lastMix = 1.0f, lastOut = 1.0f, lastIn = 1.0f;
    int lastLatency = -1, lastPhaseMode = -1, lastStages = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnjisAudioProcessor)
};
