#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "dsp/TelemetrySnapshot.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/TruePeakMeter.h"
#include "dsp/SpectralAnalyzer.h"
#include "dsp/SpatialAnalyzer.h"
#include "dsp/MasterChain.h"
#include "dsp/mix/TrackNodeTypes.h"
#include "dsp/mix/InterPluginBus.h"
#include "dsp/mix/SourceChainProcessor.h"
#include "dsp/mix/StemClassifier.h"

class AutoMixMasterProcessor : public juce::AudioProcessor {
public:
    AutoMixMasterProcessor();
    ~AutoMixMasterProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AutoMixMaster"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // Lock-free telemetry snapshot query for GUI and Headless Verifier
    dsp::TelemetrySnapshot getTelemetrySnapshot() const noexcept;

    // Direct access to master chain and analyzers
    dsp::MasterChain& getMasterChain() noexcept { return masterChain; }
    const dsp::MasterChain& getMasterChain() const noexcept { return masterChain; }

    const dsp::LoudnessMeter& getLoudnessMeter() const noexcept { return loudnessMeter; }
    const dsp::TruePeakMeter& getTruePeakMeter() const noexcept { return truePeakMeter; }
    const dsp::SpectralAnalyzer& getSpectralAnalyzer() const noexcept { return spectralAnalyzer; }
    const dsp::SpatialAnalyzer& getSpatialAnalyzer() const noexcept { return spatialAnalyzer; }

    void setProcessingEnabled(bool enabled) noexcept { processingEnabled = enabled; }
    [[nodiscard]] bool isProcessingEnabled() const noexcept { return processingEnabled; }

    // Milestone 2: Dual Operating Mode (MasterBus vs TrackNode)
    void setOperatingMode(dsp::mix::PluginOperatingMode mode) noexcept {
        operatingMode = mode;
    }
    [[nodiscard]] dsp::mix::PluginOperatingMode getOperatingMode() const noexcept {
        return operatingMode;
    }

    void setTrackClass(dsp::mix::TrackClass tClass) noexcept {
        trackClass = tClass;
        sourceChain.setTrackClass(tClass);
    }
    [[nodiscard]] dsp::mix::TrackClass getTrackClass() const noexcept {
        return trackClass;
    }

    void setTrackName(const juce::String& name) noexcept {
        trackName = name;
        if (trackClass == dsp::mix::TrackClass::Unknown) {
            trackClass = dsp::mix::StemClassifier::classifyFromTrackName(name.toStdString());
            sourceChain.setTrackClass(trackClass);
        }
    }
    [[nodiscard]] const juce::String& getTrackName() const noexcept { return trackName; }

    dsp::mix::SourceChainProcessor& getSourceChain() noexcept { return sourceChain; }
    const dsp::mix::SourceChainProcessor& getSourceChain() const noexcept { return sourceChain; }

    [[nodiscard]] int getInterPluginSlot() const noexcept { return interPluginSlot; }

    // Milestone 3A: Level-Matched A/B and Delta Monitoring Controls
    void setLevelMatchedAB(bool enabled) noexcept { levelMatchedAB.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool isLevelMatchedAB() const noexcept { return levelMatchedAB.load(std::memory_order_relaxed); }

    void setListenBypassA(bool listenA) noexcept { listenBypassA.store(listenA, std::memory_order_relaxed); }
    [[nodiscard]] bool isListeningToDryA() const noexcept { return listenBypassA.load(std::memory_order_relaxed); }

    void setDeltaListening(bool delta) noexcept { deltaListening.store(delta, std::memory_order_relaxed); }
    [[nodiscard]] bool isDeltaListening() const noexcept { return deltaListening.load(std::memory_order_relaxed); }

    // Milestone 3A: 4 Flagship Macro Controls
    void setMacroStyle(float val) noexcept { macroStyle.store(std::clamp(val, 0.0f, 1.0f), std::memory_order_relaxed); }
    [[nodiscard]] float getMacroStyle() const noexcept { return macroStyle.load(std::memory_order_relaxed); }

    void setMacroIntensity(float val) noexcept { macroIntensity.store(std::clamp(val, 0.0f, 1.0f), std::memory_order_relaxed); }
    [[nodiscard]] float getMacroIntensity() const noexcept { return macroIntensity.load(std::memory_order_relaxed); }

    void setMacroPunchGlue(float val) noexcept { macroPunchGlue.store(std::clamp(val, 0.0f, 1.0f), std::memory_order_relaxed); }
    [[nodiscard]] float getMacroPunchGlue() const noexcept { return macroPunchGlue.load(std::memory_order_relaxed); }

    void setMacroClarity(float val) noexcept { macroClarity.store(std::clamp(val, 0.0f, 1.0f), std::memory_order_relaxed); }
    [[nodiscard]] float getMacroClarity() const noexcept { return macroClarity.load(std::memory_order_relaxed); }

    void resetAnalyzers() noexcept {
        loudnessMeter.reset();
        truePeakMeter.reset();
        spectralAnalyzer.reset();
        spatialAnalyzer.reset();
        masterChain.reset();
        sourceChain.reset();
        fifoBuffer.clear();
        fifoWritePos = 0;
    }

private:
    static constexpr int kInternalFifoCapacity = 16384;

    double currentSampleRate = 48000.0;
    std::atomic<bool> sidechainActive { false };
    bool processingEnabled = true;

    // Milestone 3A Monitoring & Macros
    std::atomic<bool>  levelMatchedAB { false };
    std::atomic<bool>  listenBypassA  { false };
    std::atomic<bool>  deltaListening { false };

    std::atomic<float> macroStyle     { 0.5f }; // 0 = Warm, 0.5 = Balanced, 1 = Open
    std::atomic<float> macroIntensity { 1.0f }; // 0 to 1.0
    std::atomic<float> macroPunchGlue { 0.5f }; // 0 to 1.0
    std::atomic<float> macroClarity   { 0.5f }; // 0 to 1.0

    // Pre-allocated dry buffer for zero-alloc level-matched A/B & delta listening
    juce::AudioBuffer<float> dryBuffer;

    // Milestone 2 Mode & Stem Chain
    dsp::mix::PluginOperatingMode  operatingMode = dsp::mix::PluginOperatingMode::MasterBus;
    dsp::mix::TrackClass           trackClass = dsp::mix::TrackClass::Unknown;
    juce::String                   trackName;
    dsp::mix::SourceChainProcessor sourceChain;
    int                            interPluginSlot = -1;
    juce::AudioBuffer<float>       tempSidechainBuffer;

    // 10-Stage Mastering Chain
    dsp::MasterChain     masterChain;

    // DSP Analyzers & Metering Engines
    dsp::LoudnessMeter   loudnessMeter;
    dsp::TruePeakMeter   truePeakMeter;
    dsp::SpectralAnalyzer spectralAnalyzer;
    dsp::SpatialAnalyzer  spatialAnalyzer;

    // Pre-allocated circular ring buffer for FL Studio variable buffer decoupling
    juce::AudioBuffer<float> fifoBuffer;
    int fifoWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoMixMasterProcessor)
};
