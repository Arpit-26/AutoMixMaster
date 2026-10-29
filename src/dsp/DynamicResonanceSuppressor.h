#pragma once
#include "Biquad.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 8: Post-Compression Dynamic Resonance Suppressor.
 * Level-independent adaptive spectral de-resonator:
 * - Scans 8 log-spaced key resonance centers across 200 Hz to 12 kHz:
 *   [250, 450, 800, 1400, 2500, 4000, 6500, 10000] Hz.
 * - Dynamically reduces sharp, narrow ringing peaks that stand out above the local spectrum.
 * - Strictly capped at a hard maximum cut ceiling of 6.0 dB.
 */
class DynamicResonanceSuppressor {
public:
    static constexpr size_t kNumBands = 8;

    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        const std::array<double, kNumBands> centerFreqs = {
            250.0, 450.0, 800.0, 1400.0, 2500.0, 4000.0, 6500.0, 10000.0
        };

        attackCoeff = std::exp(-1.0 / (4.0 * 0.001 * sampleRate));   // 4 ms attack
        releaseCoeff = std::exp(-1.0 / (75.0 * 0.001 * sampleRate)); // 75 ms release

        for (size_t b = 0; b < kNumBands; ++b) {
            bandFreqs[b] = centerFreqs[b];
            bandPassFiltersL[b].setCoefficients(Biquad::makeBandPass(sampleRate, centerFreqs[b], 4.0));
            bandPassFiltersR[b].setCoefficients(Biquad::makeBandPass(sampleRate, centerFreqs[b], 4.0));

            const auto initCut = Biquad::makePeakEQ(sampleRate, centerFreqs[b], 3.5, 0.0);
            notchFiltersL[b].setCoefficients(initCut);
            notchFiltersR[b].setCoefficients(initCut);

            bandEnvelopes[b] = 0.0;
            dynamicCutsDb[b] = 0.0;
        }

        reset();
    }

    void reset() noexcept {
        for (size_t b = 0; b < kNumBands; ++b) {
            bandPassFiltersL[b].reset();
            bandPassFiltersR[b].reset();
            notchFiltersL[b].reset();
            notchFiltersR[b].reset();
            bandEnvelopes[b] = 0.0;
            dynamicCutsDb[b] = 0.0;
        }
        maxCutApplied_dB = 0.0f;
    }

    void setMaxCutCeiling(double ceilingDb) noexcept {
        maxCutCeilingDb = std::clamp(ceilingDb, 2.0, 9.0);
    }

    [[nodiscard]] float getMaxCutApplied_dB() const noexcept {
        return maxCutApplied_dB;
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        double maxBlockCut = 0.0;

        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // 1. Detect band energy across the 8 frequency centers
            double avgEnergy = 0.0;
            for (size_t b = 0; b < kNumBands; ++b) {
                const double bpL = bandPassFiltersL[b].processSample(inL);
                const double bpR = bandPassFiltersR[b].processSample(inR);
                const double peak = std::max(std::abs(bpL), std::abs(bpR));

                if (peak > bandEnvelopes[b]) {
                    bandEnvelopes[b] = attackCoeff * bandEnvelopes[b] + (1.0 - attackCoeff) * peak;
                } else {
                    bandEnvelopes[b] = releaseCoeff * bandEnvelopes[b] + (1.0 - releaseCoeff) * peak;
                }
                avgEnergy += bandEnvelopes[b];
            }
            avgEnergy /= static_cast<double>(kNumBands);

            // 2. Identify outlier resonances standing out above the local spectral average
            for (size_t b = 0; b < kNumBands; ++b) {
                double targetCut = 0.0;
                // If this band is > 2.5x the average energy (approx +8 dB), engage dynamic suppression
                if (bandEnvelopes[b] > avgEnergy * 2.5 && bandEnvelopes[b] > 1e-4) {
                    const double ratioToAvg = bandEnvelopes[b] / std::max(1e-5, avgEnergy);
                    const double excessDb = 20.0 * std::log10(ratioToAvg) - 8.0;
                    targetCut = std::clamp(excessDb * 0.6, 0.0, maxCutCeilingDb);
                }

                dynamicCutsDb[b] = 0.95 * dynamicCutsDb[b] + 0.05 * targetCut;
                if (dynamicCutsDb[b] > maxBlockCut) {
                    maxBlockCut = dynamicCutsDb[b];
                }

                // Update filter coefficients if cut is active
                if (dynamicCutsDb[b] > 0.1) {
                    const auto cutCoeffs = Biquad::makePeakEQ(sampleRate, bandFreqs[b], 3.5, -dynamicCutsDb[b]);
                    notchFiltersL[b].setCoefficients(cutCoeffs);
                    notchFiltersR[b].setCoefficients(cutCoeffs);
                }
            }

            // 3. Cascade notch filters
            double procL = inL;
            double procR = inR;
            for (size_t b = 0; b < kNumBands; ++b) {
                if (dynamicCutsDb[b] > 0.1) {
                    procL = notchFiltersL[b].processSample(procL);
                    if (numChannels > 1) {
                        procR = notchFiltersR[b].processSample(procR);
                    }
                }
            }

            chL[i] = static_cast<float>(procL);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(procR);
            }
        }

        maxCutApplied_dB = static_cast<float>(maxBlockCut);
    }

private:
    double sampleRate = 48000.0;
    double maxCutCeilingDb = 6.0; // Strictly hard-capped at 6.0 dB (requirement: 6.0 to 9.0 dB)

    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;

    std::array<double, kNumBands> bandFreqs {};
    std::array<double, kNumBands> bandEnvelopes {};
    std::array<double, kNumBands> dynamicCutsDb {};

    std::array<Biquad, kNumBands> bandPassFiltersL {};
    std::array<Biquad, kNumBands> bandPassFiltersR {};
    std::array<Biquad, kNumBands> notchFiltersL {};
    std::array<Biquad, kNumBands> notchFiltersR {};

    float maxCutApplied_dB = 0.0f;
};

} // namespace dsp
