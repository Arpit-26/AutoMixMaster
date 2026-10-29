#pragma once
#include "Biquad.h"
#include <array>
#include <algorithm>

namespace dsp {

/**
 * Stage 2: Corrective EQ.
 * Provides up to 4 surgical narrow notch/bell filters with high Q (6.0 - 12.0)
 * hard-capped at <= 3.0 dB of reduction to clean up sharp resonant ringing
 * without hollowing out master program material.
 */
class CorrectiveEQ {
public:
    static constexpr size_t kMaxFilters = 4;

    struct NotchParams {
        double freqHz = 1000.0;
        double q = 8.0;
        double cutDb = 0.0; // 0.0 to 3.0 dB of cut
        bool enabled = false;
    };

    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        for (size_t i = 0; i < kMaxFilters; ++i) {
            filtersL[i].reset();
            filtersR[i].reset();
        }
        updateCoefficients();
    }

    void reset() noexcept {
        for (size_t i = 0; i < kMaxFilters; ++i) {
            filtersL[i].reset();
            filtersR[i].reset();
        }
    }

    void setNotch(size_t index, double freqHz, double cutDb, double q = 8.0) noexcept {
        if (index >= kMaxFilters) return;
        // Strictly cap cut at 3.0 dB max on the master bus
        const double clampedCut = std::clamp(cutDb, 0.0, 3.0);
        notches[index].freqHz = std::clamp(freqHz, 40.0, sampleRate * 0.45);
        notches[index].q = std::clamp(q, 4.0, 16.0);
        notches[index].cutDb = clampedCut;
        notches[index].enabled = (clampedCut > 0.05);

        updateFilter(index);
    }

    void clearAllNotches() noexcept {
        for (size_t i = 0; i < kMaxFilters; ++i) {
            notches[i].enabled = false;
            notches[i].cutDb = 0.0;
            updateFilter(i);
        }
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        for (size_t f = 0; f < kMaxFilters; ++f) {
            if (!notches[f].enabled) continue;

            for (int i = 0; i < numSamples; ++i) {
                chL[i] = filtersL[f].processSample(chL[i]);
                if (numChannels > 1) {
                    chR[i] = filtersR[f].processSample(chR[i]);
                }
            }
        }
    }

private:
    void updateFilter(size_t index) noexcept {
        if (index >= kMaxFilters) return;
        if (notches[index].enabled) {
            // Cut is positive value in dB, so gain is -cutDb
            const auto coeffs = Biquad::makePeakEQ(sampleRate, notches[index].freqHz,
                                                   notches[index].q, -notches[index].cutDb);
            filtersL[index].setCoefficients(coeffs);
            filtersR[index].setCoefficients(coeffs);
        } else {
            BiquadCoeffs bypassCoeffs { 1.0, 0.0, 0.0, 0.0, 0.0 };
            filtersL[index].setCoefficients(bypassCoeffs);
            filtersR[index].setCoefficients(bypassCoeffs);
        }
    }

    void updateCoefficients() noexcept {
        for (size_t i = 0; i < kMaxFilters; ++i) {
            updateFilter(i);
        }
    }

    double sampleRate = 48000.0;
    std::array<NotchParams, kMaxFilters> notches {};
    std::array<Biquad, kMaxFilters> filtersL {};
    std::array<Biquad, kMaxFilters> filtersR {};
};

} // namespace dsp
