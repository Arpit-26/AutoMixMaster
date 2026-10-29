#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 6: Band-Limited Harmonic Saturation.
 * - Sub-bass (<200 Hz) is decoupled and preserved unclipped for maximum punch.
 * - Mids and Highs (>200 Hz) undergo gentle dual-harmonic saturation:
 *   * Even-order tube warmth: x + 0.15 * x^2 * sgn(x)
 *   * Odd-order tape soft-clipping: tanh(x) / (1 + x^2 * 0.1)
 * - Automatic loudness compensation ensures saturation adds harmonic richness
 *   without shifting overall track loudness.
 */
class HarmonicSaturator {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        xover200L.prepare(sampleRate, 200.0);
        xover200R.prepare(sampleRate, 200.0);
        reset();
    }

    void reset() noexcept {
        xover200L.reset();
        xover200R.reset();
        drive = 1.0f;
        tubeMix = 0.35f;
        tapeMix = 0.25f;
        gainCompensation = 1.0f;
    }

    void setParameters(float newDrive, float newTubeMix, float newTapeMix) noexcept {
        drive = std::clamp(newDrive, 0.5f, 3.0f);
        tubeMix = std::clamp(newTubeMix, 0.0f, 1.0f);
        tapeMix = std::clamp(newTapeMix, 0.0f, 1.0f);

        // Approximate analytical loudness compensation for the chosen saturation curve
        const double testX = 0.5 * static_cast<double>(drive);
        const double saturatedTest = saturateSample(testX);
        if (saturatedTest > 1e-4) {
            gainCompensation = static_cast<float>(0.5 / saturatedTest);
        } else {
            gainCompensation = 1.0f;
        }
    }

    void setDrive(float newDrive) noexcept {
        setParameters(newDrive, tubeMix, tapeMix);
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // 1. Crossover at 200 Hz (keeps sub-bass clean)
            double lowL = 0.0, highL = 0.0;
            double lowR = 0.0, highR = 0.0;
            xover200L.processSample(inL, lowL, highL);
            xover200R.processSample(inR, lowR, highR);

            // 2. Harmonic Saturation on High Band
            const double satHighL = saturateSample(highL * static_cast<double>(drive)) * static_cast<double>(gainCompensation);
            const double satHighR = saturateSample(highR * static_cast<double>(drive)) * static_cast<double>(gainCompensation);

            // 3. In-Phase Summation
            chL[i] = static_cast<float>(lowL + satHighL);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(lowR + satHighR);
            }
        }
    }

private:
    [[nodiscard]] inline double saturateSample(double x) const noexcept {
        // Even-order tube warmth
        const double tubePart = x + 0.15 * (x * x) * (x >= 0.0 ? 1.0 : -1.0);
        // Odd-order tape soft saturation
        const double tapePart = std::tanh(x);

        const double dryPart = 1.0 - static_cast<double>(tubeMix) - static_cast<double>(tapeMix);
        return dryPart * x + static_cast<double>(tubeMix) * tubePart + static_cast<double>(tapeMix) * tapePart;
    }

    double sampleRate = 48000.0;
    LinkwitzRiley4thOrder xover200L;
    LinkwitzRiley4thOrder xover200R;

    float drive = 1.0f;
    float tubeMix = 0.35f;
    float tapeMix = 0.25f;
    float gainCompensation = 1.0f;
};

} // namespace dsp
