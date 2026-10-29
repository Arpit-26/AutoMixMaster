#pragma once
#include "KWeightingFilter.h"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace dsp {

struct LoudnessResult {
    float momentaryLUFS       = -70.0f;
    float shortTermLUFS       = -70.0f;
    float maxShortTermLUFS    = -70.0f;
    float integratedLUFS      = -70.0f;
    float loudnessRangeLU     = 0.0f;
    float plrLU               = 0.0f;
    float minPsrLU            = 100.0f;
};

/**
 * ITU-R BS.1770-4 & EBU Tech 3342 Compliant Loudness Meter.
 * Computes Momentary (400ms), Short-Term (3s), Gated Integrated Loudness,
 * Loudness Range (LRA), and localized PSR (in loudest 30s section).
 * Pre-allocates all telemetry history in prepare() to ensure zero audio-thread allocations.
 */
class LoudnessMeter {
public:
    LoudnessMeter() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize, double maxDurationSeconds = 3600.0);
    void reset() noexcept;

    // Real-time audio processing: zero heap allocations
    void processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept;

    // Loudness calculation & snapshot queries
    [[nodiscard]] float getMomentaryLUFS() const noexcept {
        return atomicMomentary.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getShortTermLUFS() const noexcept {
        return atomicShortTerm.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getMaxShortTermLUFS() const noexcept {
        return atomicMaxShortTerm.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getIntegratedLUFS() const noexcept {
        return atomicIntegrated.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getLoudnessRangeLU() const noexcept {
        return atomicLRA.load(std::memory_order_relaxed);
    }

    // Comprehensive offline / session evaluation
    LoudnessResult calculateFinalTelemetry(float currentTruePeakdBTP) const;

private:
    void processHop(double energyL, double energyR) noexcept;

    double sampleRate = 48000.0;
    int hopSamples100ms = 4800;
    int sampleCounter = 0;
    double accumEnergyL = 0.0;
    double accumEnergyR = 0.0;

    std::array<KWeightingFilter, 2> kFilters;

    // Sliding ring buffer for 400ms Momentary (4 hops of 100ms)
    std::array<double, 4> momentaryRingBuffer {};
    int momentaryRingIdx = 0;
    int momentaryHopCount = 0;

    // Sliding ring buffer for 3000ms Short-Term (30 hops of 100ms)
    std::array<double, 30> shortTermRingBuffer {};
    int shortTermRingIdx = 0;
    int shortTermHopCount = 0;

    // Pre-allocated hop history for gated integrated loudness & LRA
    std::vector<double> recordedHopEnergies;
    size_t recordedHopCount = 0;

    std::vector<double> recordedShortTermLoudness;
    size_t recordedShortTermCount = 0;

    // Lock-free atomic outputs
    std::atomic<float> atomicMomentary    { -70.0f };
    std::atomic<float> atomicShortTerm    { -70.0f };
    std::atomic<float> atomicMaxShortTerm { -70.0f };
    std::atomic<float> atomicIntegrated   { -70.0f };
    std::atomic<float> atomicLRA          { 0.0f };
};

} // namespace dsp
