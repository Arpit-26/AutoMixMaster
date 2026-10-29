#pragma once
#include "Biquad.h"
#include <array>
#include <atomic>
#include <cmath>

namespace dsp {

struct SpatialMetrics {
    std::array<float, 4> bandCorrelation = { 1.0f, 1.0f, 1.0f, 1.0f }; // [<120Hz, 120-1k, 1k-5k, >5k]
    float globalCorrelation = 1.0f;
    float subMonoSumLoss_dB = 0.0f;
    bool  monoIncompatible  = false;
};

/**
 * 4-Band Phase Correlation & Sub-Bass Mono Compatibility Analyzer.
 * Uses a tree of 4th-order Linkwitz-Riley crossovers (120 Hz, 1000 Hz, 5000 Hz).
 * Evaluates correlation across all 4 macro frequency regions and detects
 * sub-bass (<120 Hz) mono-sum cancellation loss exceeding the 1.0 dB threshold.
 */
class SpatialAnalyzer {
public:
    SpatialAnalyzer() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // Real-time audio processing: zero heap allocations
    void processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept;

    SpatialMetrics getMetrics() const noexcept;

private:
    double sampleRate = 48000.0;

    // 4th order Linkwitz-Riley crossover tree for Left channel
    LinkwitzRiley4thOrder xover120L;
    LinkwitzRiley4thOrder xover1000L;
    LinkwitzRiley4thOrder xover5000L;

    // 4th order Linkwitz-Riley crossover tree for Right channel
    LinkwitzRiley4thOrder xover120R;
    LinkwitzRiley4thOrder xover1000R;
    LinkwitzRiley4thOrder xover5000R;

    // Band energy & cross-product accumulators
    struct BandAccum {
        double dotLR = 0.0;
        double sumLL = 0.0;
        double sumRR = 0.0;
    };
    std::array<BandAccum, 4> bandAccums;
    BandAccum globalAccum;

    // Sub-bass mono loss accumulator (<120 Hz)
    double subStereoEnergy = 0.0;
    double subMonoEnergy   = 0.0;

    int sampleCounter = 0;
    int updateInterval = 2048; // Updated every ~43ms at 48kHz

    // Lock-free atomic readouts
    std::array<std::atomic<float>, 4> atomicBandCorrelation;
    std::atomic<float> atomicGlobalCorrelation { 1.0f };
    std::atomic<float> atomicSubMonoLoss_dB    { 0.0f };
    std::atomic<bool>  atomicMonoIncompatible  { false };
};

} // namespace dsp
