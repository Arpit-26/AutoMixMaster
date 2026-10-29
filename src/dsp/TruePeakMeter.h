#pragma once
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace dsp {

enum class TruePeakMode {
    ITU_Reference_48Tap,
    Mastering_Kaiser_128Tap
};

/**
 * 4x Polyphase FIR Interpolation True-Peak Meter.
 * Pre-computes 4 polyphase subfilter branches in prepare() and maintains
 * zero-allocation per-channel delay lines for real-time sample processing.
 * Conforms to ITU-R BS.1770-4 Annex 2 True Peak specifications.
 */
class TruePeakMeter {
public:
    TruePeakMeter() noexcept = default;

    void prepare(double sampleRate, int numChannels, TruePeakMode mode = TruePeakMode::Mastering_Kaiser_128Tap);
    void reset() noexcept;

    // Real-time audio thread processing: zero heap allocations
    void processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept;

    [[nodiscard]] float getMaxPeakVal() const noexcept {
        return peakValue.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getMaxPeakdBTP() const noexcept {
        const float val = peakValue.load(std::memory_order_relaxed);
        return (val > 1.0e-9f) ? (20.0f * std::log10(val)) : -100.0f;
    }

    void resetPeak() noexcept {
        peakValue.store(0.0f, std::memory_order_relaxed);
    }

private:
    void generatePolyphaseFilters(TruePeakMode mode);
    static double besselI0(double x) noexcept;

    TruePeakMode currentMode = TruePeakMode::Mastering_Kaiser_128Tap;
    int tapsPerPhase = 32; // 128 total / 4 = 32 for Kaiser, 48 / 4 = 12 for ITU
    int activeChannels = 2;

    // 4 polyphase subfilter branches
    std::array<std::vector<float>, 4> polyphaseBranches;

    // Per-channel circular delay lines for input history
    std::vector<std::vector<float>> channelDelayLines;
    std::vector<int> delayLineIndices;

    std::atomic<float> peakValue { 0.0f };
};

} // namespace dsp
