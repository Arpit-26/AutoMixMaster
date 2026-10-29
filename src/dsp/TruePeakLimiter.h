#pragma once
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <algorithm>

namespace dsp {

/**
 * Stage 10: 4x Polyphase FIR Lookahead True-Peak Limiter & Export Dithering.
 * - 4x Polyphase FIR True Peak oversampled peak detector (128-tap Kaiser beta=8.5).
 * - Pre-allocated lookahead delay buffer (5 ms) for zero overshoot.
 * - Smooth exponential attack and program-dependent dual release (fast transient recovery + musical sustain).
 * - High-pass TPDF dithering for 16-bit export quantization.
 * - Zero allocations on audio thread.
 */
class TruePeakLimiter {
public:
    TruePeakLimiter() noexcept = default;

    void prepare(double sampleRate, int numChannels = 2, double lookaheadMs = 5.0);
    void reset() noexcept;

    void setCeilingdBTP(double ceilingDb) noexcept;
    void setReleaseMs(double releaseMs) noexcept;
    void setDitherEnabled(bool enabled) noexcept;

    [[nodiscard]] int getLatencySamples() const noexcept { return lookaheadSamples; }
    [[nodiscard]] float getGainReduction_dB() const noexcept { return currentGrDb; }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept;

private:
    void generatePolyphaseFilters();
    static double besselI0(double x) noexcept;

    double sampleRate = 48000.0;
    int activeChannels = 2;
    int lookaheadSamples = 240; // ~5 ms at 48 kHz
    int delayBufferSize = 2048;

    double ceilingDbTP = -1.0;
    float ceilingLinear = 0.89125f;

    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    double fastReleaseCoeff = 0.0;

    double envelope = 1.0;
    float currentGrDb = 0.0f;
    float maxGrSessionDb = 0.0f;

    // Lookahead audio delay ring buffers [channel][sample]
    std::vector<std::vector<float>> delayBuffers;
    int writeIndex = 0;

    // FIR Polyphase 4x Oversampling
    static constexpr int kNumPhases = 4;
    int tapsPerPhase = 32; // 128 taps total / 4 phases
    std::vector<std::vector<float>> polyphaseCoeffs; // [phase][tap]
    std::vector<std::vector<float>> firHistory;      // [channel][tap]
    int firHistoryIdx = 0;

    // Dither
    bool ditherEnabled = false;
    uint32_t ditherRngState = 0x12345678;
    float lastDitherNoiseL = 0.0f;
    float lastDitherNoiseR = 0.0f;
};

} // namespace dsp
