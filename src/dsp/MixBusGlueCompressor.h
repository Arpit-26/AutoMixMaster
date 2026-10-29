#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 3: Mix-Bus Glue Compressor.
 * Transparent VCA-style master bus glue compressor featuring:
 * - 100 Hz high-pass sidechain filter to decouple kick/bass energy from mix pumping.
 * - Gentle ratio (1.5:1 to 2:1) and wide soft knee (3 dB).
 * - Slow punch-preserving attack (15-30 ms).
 * - Program-dependent dual-release ballistics targeting 1.0 - 2.0 dB of transparent glue.
 */
class MixBusGlueCompressor {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        scHpfL.prepare(sampleRate, 100.0);
        scHpfR.prepare(sampleRate, 100.0);

        setParameters(-14.0, 1.75, 20.0, 150.0);
        reset();
    }

    void reset() noexcept {
        scHpfL.reset();
        scHpfR.reset();
        envelope = 1.0;
        smoothedGain = 1.0;
        currentGainReduction_dB = 0.0f;
        maxGrSessionDb = 0.0f;
    }

    void setParameters(double threshDb, double rat, double attackMs, double releaseMs) noexcept {
        thresholdDb = threshDb;
        ratio = std::clamp(rat, 1.2, 3.0);
        attackCoeff = std::exp(-1.0 / (std::max(1.0, attackMs) * 0.001 * sampleRate));
        releaseCoeff = std::exp(-1.0 / (std::max(10.0, releaseMs) * 0.001 * sampleRate));
        fastReleaseCoeff = std::exp(-1.0 / (40.0 * 0.001 * sampleRate));
    }

    void setThreshold(double threshDb) noexcept {
        thresholdDb = threshDb;
    }

    [[nodiscard]] float getGainReduction_dB() const noexcept {
        return currentGainReduction_dB;
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        double maxGrBlock = 0.0;

        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // 1. Sidechain high-pass filtering (100 Hz 4th-order)
            double lowL = 0.0, scL = 0.0;
            double lowR = 0.0, scR = 0.0;
            scHpfL.processSample(inL, lowL, scL);
            scHpfR.processSample(inR, lowR, scR);

            // 2. Sidechain peak level detection
            const double scLevel = std::max(std::abs(scL), std::abs(scR));
            const double scDb = (scLevel > 1e-6) ? 20.0 * std::log10(scLevel) : -120.0;

            // 3. Static Gain Computer with 3 dB Soft Knee
            constexpr double kneeWidthDb = 3.0;
            const double halfKnee = kneeWidthDb * 0.5;
            double grDb = 0.0;

            if (scDb > thresholdDb + halfKnee) {
                // Above knee
                grDb = (1.0 - 1.0 / ratio) * (scDb - thresholdDb);
            } else if (scDb > thresholdDb - halfKnee) {
                // In knee
                const double delta = scDb - (thresholdDb - halfKnee);
                grDb = (1.0 - 1.0 / ratio) * (delta * delta) / (2.0 * kneeWidthDb);
            }

            // 4. Ballistics Smoothing (Decoupled Attack & Program-Dependent Release)
            const double targetGainLin = std::pow(10.0, -grDb / 20.0);
            if (targetGainLin < envelope) {
                // Attack (gain falling)
                envelope = attackCoeff * envelope + (1.0 - attackCoeff) * targetGainLin;
            } else {
                // Release (gain rising) - fast recovery for light transients, slower for heavy compression
                const double effRelCoeff = (envelope < 0.85) ? releaseCoeff : fastReleaseCoeff;
                envelope = effRelCoeff * envelope + (1.0 - effRelCoeff) * targetGainLin;
            }

            smoothedGain = envelope;
            const double currentGr = (smoothedGain < 0.9999) ? -20.0 * std::log10(smoothedGain) : 0.0;
            if (currentGr > maxGrBlock) {
                maxGrBlock = currentGr;
            }

            // 5. Apply gain reduction
            chL[i] = static_cast<float>(inL * smoothedGain);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(inR * smoothedGain);
            }
        }

        if (static_cast<float>(maxGrBlock) > maxGrSessionDb) {
            maxGrSessionDb = static_cast<float>(maxGrBlock);
        }
        currentGainReduction_dB = maxGrSessionDb;
    }

private:
    double sampleRate = 48000.0;
    LinkwitzRiley4thOrder scHpfL;
    LinkwitzRiley4thOrder scHpfR;

    double thresholdDb = -14.0;
    double ratio = 1.75;
    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    double fastReleaseCoeff = 0.0;

    double envelope = 1.0;
    double smoothedGain = 1.0;
    float currentGainReduction_dB = 0.0f;
    float maxGrSessionDb = 0.0f;
};

} // namespace dsp
