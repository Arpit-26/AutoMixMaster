#include "PluginProcessor.h"
#include <cmath>

AutoMixMasterProcessor::AutoMixMasterProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Main Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Main Output", juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain",   juce::AudioChannelSet::stereo(), false)) // FL Studio Wrapper Aux Sidechain
{
}

bool AutoMixMasterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // 1. Enforce Main stereo input and output
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }

    // 2. Validate FL Studio Sidechain Bus (Bus 1)
    const auto scSet = layouts.getChannelSet(true, 1);
    return scSet.isDisabled()
        || scSet == juce::AudioChannelSet::mono()
        || scSet == juce::AudioChannelSet::stereo();
}

void AutoMixMasterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;

    // Initialize all telemetry engines (zero allocation during processBlock)
    loudnessMeter.prepare(sampleRate, samplesPerBlock, 3600.0);
    truePeakMeter.prepare(sampleRate, 2, dsp::TruePeakMode::Mastering_Kaiser_128Tap);
    spectralAnalyzer.prepare(sampleRate, samplesPerBlock);
    spatialAnalyzer.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate circular ring buffer for FL Studio variable-sized blocks
    fifoBuffer.setSize(2, kInternalFifoCapacity, false, true, false);
    fifoBuffer.clear();
    fifoWritePos = 0;

    // Report exact oversampling interpolation delay to FL Studio PDC (16 samples at 4x for 128 taps)
    setLatencySamples(16);
}

void AutoMixMasterProcessor::releaseResources() {
    loudnessMeter.reset();
    truePeakMeter.reset();
    spectralAnalyzer.reset();
    spatialAnalyzer.reset();
    fifoBuffer.clear();
}

void AutoMixMasterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer(buffer, true, 0);
    const int numSamples = mainBus.getNumSamples();
    if (numSamples == 0) {
        return;
    }

    // 1. Detect FL Studio Auxiliary Sidechain Bus activity (zero heap allocation)
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) {
        auto scBus = getBusBuffer(buffer, true, 1);
        const float scRms = scBus.getRMSLevel(0, 0, numSamples);
        sidechainActive.store(scRms > 1.0e-5f, std::memory_order_relaxed);
    } else {
        sidechainActive.store(false, std::memory_order_relaxed);
    }

    // 2. Push input samples into pre-allocated circular ring buffer
    const float* const inL = mainBus.getReadPointer(0);
    const float* const inR = (mainBus.getNumChannels() > 1) ? mainBus.getReadPointer(1) : inL;

    for (int i = 0; i < numSamples; ++i) {
        fifoBuffer.setSample(0, fifoWritePos, inL[i]);
        fifoBuffer.setSample(1, fifoWritePos, inR[i]);
        fifoWritePos = (fifoWritePos + 1) % kInternalFifoCapacity;
    }

    const float* const channelPointers[2] = { inL, inR };

    // 3. Process Telemetry Engines
    loudnessMeter.processBlock(channelPointers, 2, numSamples);
    truePeakMeter.processBlock(channelPointers, 2, numSamples);
    spectralAnalyzer.processBlock(channelPointers, 2, numSamples);
    spatialAnalyzer.processBlock(channelPointers, 2, numSamples);
}

dsp::TelemetrySnapshot AutoMixMasterProcessor::getTelemetrySnapshot() const noexcept {
    dsp::TelemetrySnapshot snap;

    // Loudness
    snap.momentaryLUFS    = loudnessMeter.getMomentaryLUFS();
    snap.shortTermLUFS    = loudnessMeter.getShortTermLUFS();
    snap.maxShortTermLUFS = loudnessMeter.getMaxShortTermLUFS();
    snap.integratedLUFS   = loudnessMeter.getIntegratedLUFS();
    snap.loudnessRangeLU  = loudnessMeter.getLoudnessRangeLU();
    snap.truePeakdBTP     = truePeakMeter.getMaxPeakdBTP();
    snap.plrLU            = snap.truePeakdBTP - snap.integratedLUFS;
    snap.minPsrLU         = snap.truePeakdBTP - snap.maxShortTermLUFS;

    // Spectral
    const auto spectral = spectralAnalyzer.getScoring();
    snap.macroBandEnergies_dB[0] = spectral.bass_dB;
    snap.macroBandEnergies_dB[1] = spectral.lowMids_dB;
    snap.macroBandEnergies_dB[2] = spectral.highMids_dB;
    snap.macroBandEnergies_dB[3] = spectral.treble_dB;
    snap.spectralTiltSlope       = spectral.spectralTilt_dBoct;
    snap.lowBandCrest_dB         = spectral.lowBandCrest_dB;

    // Spatial
    const auto spatial = spatialAnalyzer.getMetrics();
    snap.bandCorrelation   = spatial.bandCorrelation;
    snap.globalCorrelation = spatial.globalCorrelation;
    snap.subMonoSumLoss_dB = spatial.subMonoSumLoss_dB;
    snap.monoIncompatibility = spatial.monoIncompatible;

    // Sidechain
    snap.sidechainActive = sidechainActive.load(std::memory_order_relaxed);

    return snap;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AutoMixMasterProcessor();
}
