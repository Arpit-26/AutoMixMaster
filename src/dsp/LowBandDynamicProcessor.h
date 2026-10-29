#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 4: Low-Band Crest Multiband Compressor (<250 Hz) & Dynamic EQ (2-5 kHz).
 * - Splits audio at 250 Hz with a 4th-order Linkwitz-Riley crossover.
 * - Compresses high-crest sub-bass peaks (<250 Hz) prior to the limiter to prevent pumping.
 * - Applies level-dependent Dynamic EQ in the 2.5 - 4.5 kHz harshness corridor.
 */
class LowBandDynamicProcessor {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        xover250L.prepare(sampleRate, 250.0);
        xover250R.prepare(sampleRate, 250.0);

        dynamicEqFilterL.reset();
        dynamicEqFilterR.reset();

        setParameters(-16.0, 2.0, 8.0, 80.0);
        reset();
    }

    void reset() noexcept {
        xover250L.reset();
        xover250R.reset();
        dynamicEqFilterL.reset();
        dynamicEqFilterR.reset();
        lowEnvelope = 1.0;
        harshEnvelope = 0.0;
        lowGainReduction_dB = 0.0f;
        dynamicEqCut_dB = 0.0f;
    }

    void setParameters(double threshDb, double rat, double attackMs, double releaseMs) noexcept {
        lowThresholdDb = threshDb;
        lowRatio = std::clamp(rat, 1.2, 4.0);
        lowAttackCoeff = std::exp(-1.0 / (std::max(1.0, attackMs) * 0.001 * sampleRate));
        lowReleaseCoeff = std::exp(-1.0 / (std::max(10.0, releaseMs) * 0.001 * sampleRate));

        harshAttackCoeff = std::exp(-1.0 / (5.0 * 0.001 * sampleRate));   // 5 ms fast attack
        harshReleaseCoeff = std::exp(-1.0 / (60.0 * 0.001 * sampleRate)); // 60 ms release
    }

    void setLowBandThreshold(double threshDb) noexcept {
        lowThresholdDb = threshDb;
    }

    void setHarshnessControlEnabled(bool enabled) noexcept {
        harshnessControlEnabled = enabled;
    }

    [[nodiscard]] float getLowBandGainReduction_dB() const noexcept {
        return lowGainReduction_dB;
    }

    [[nodiscard]] float getDynamicEqCut_dB() const noexcept {
        return dynamicEqCut_dB;
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        double maxLowGrBlock = 0.0;
        double maxHarshCutBlock = 0.0;

        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // 1. Crossover at 250 Hz (Linkwitz-Riley 4th order)
            double lowL = 0.0, highL = 0.0;
            double lowR = 0.0, highR = 0.0;
            xover250L.processSample(inL, lowL, highL);
            xover250R.processSample(inR, lowR, highR);

            // 2. Low-band Peak Level & Gain Computer
            const double lowPeak = std::max(std::abs(lowL), std::abs(lowR));
            const double lowDb = (lowPeak > 1e-6) ? 20.0 * std::log10(lowPeak) : -120.0;

            double targetLowGain = 1.0;
            if (lowDb > lowThresholdDb) {
                const double overDb = (1.0 - 1.0 / lowRatio) * (lowDb - lowThresholdDb);
                targetLowGain = std::pow(10.0, -overDb / 20.0);
            }

            if (targetLowGain < lowEnvelope) {
                lowEnvelope = lowAttackCoeff * lowEnvelope + (1.0 - lowAttackCoeff) * targetLowGain;
            } else {
                lowEnvelope = lowReleaseCoeff * lowEnvelope + (1.0 - lowReleaseCoeff) * targetLowGain;
            }

            const double currentLowGr = (lowEnvelope < 0.9999) ? -20.0 * std::log10(lowEnvelope) : 0.0;
            if (currentLowGr > maxLowGrBlock) {
                maxLowGrBlock = currentLowGr;
            }

            const double procLowL = lowL * lowEnvelope;
            const double procLowR = lowR * lowEnvelope;

            // 3. Dynamic EQ on High Band (2.5 - 4.5 kHz Harshness Control)
            double procHighL = highL;
            double procHighR = highR;

            if (harshnessControlEnabled) {
                // Harshness detection bandpass centered around 3.5 kHz (Q = 1.5)
                const double highLevel = std::max(std::abs(highL), std::abs(highR));
                if (highLevel > harshEnvelope) {
                    harshEnvelope = harshAttackCoeff * harshEnvelope + (1.0 - harshAttackCoeff) * highLevel;
                } else {
                    harshEnvelope = harshReleaseCoeff * harshEnvelope + (1.0 - harshReleaseCoeff) * highLevel;
                }

                // If harsh high-mid energy exceeds -18 dBFS, dynamically cut up to 3.0 dB at 3.5 kHz
                const double harshDb = (harshEnvelope > 1e-6) ? 20.0 * std::log10(harshEnvelope) : -120.0;
                double dynCutDb = 0.0;
                if (harshDb > -18.0) {
                    dynCutDb = std::min(3.0, (harshDb - (-18.0)) * 0.4);
                }

                if (dynCutDb > maxHarshCutBlock) {
                    maxHarshCutBlock = dynCutDb;
                }

                if (dynCutDb > 0.05) {
                    const auto eqCoeffs = Biquad::makePeakEQ(sampleRate, 3500.0, 2.0, -dynCutDb);
                    dynamicEqFilterL.setCoefficients(eqCoeffs);
                    dynamicEqFilterR.setCoefficients(eqCoeffs);

                    procHighL = dynamicEqFilterL.processSample(highL);
                    procHighR = dynamicEqFilterR.processSample(highR);
                }
            }

            // 4. In-Phase Summation (Linkwitz-Riley outputs sum perfectly flat)
            chL[i] = static_cast<float>(procLowL + procHighL);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(procLowR + procHighR);
            }
        }

        lowGainReduction_dB = static_cast<float>(maxLowGrBlock);
        dynamicEqCut_dB = static_cast<float>(maxHarshCutBlock);
    }

private:
    double sampleRate = 48000.0;
    LinkwitzRiley4thOrder xover250L;
    LinkwitzRiley4thOrder xover250R;

    Biquad dynamicEqFilterL;
    Biquad dynamicEqFilterR;

    double lowThresholdDb = -16.0;
    double lowRatio = 2.0;
    double lowAttackCoeff = 0.0;
    double lowReleaseCoeff = 0.0;

    double harshAttackCoeff = 0.0;
    double harshReleaseCoeff = 0.0;

    double lowEnvelope = 1.0;
    double harshEnvelope = 0.0;

    float lowGainReduction_dB = 0.0f;
    float dynamicEqCut_dB = 0.0f;
    bool harshnessControlEnabled = true;
};

} // namespace dsp
