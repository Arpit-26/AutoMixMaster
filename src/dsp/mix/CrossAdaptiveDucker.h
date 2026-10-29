#pragma once
#include "../Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp::mix {

/**
 * Cross-Adaptive Spectral Masking Processor (XA-DAFx).
 * Provides intelligent, frequency-targeted dynamic EQ sidechain ducking:
 *  1. Kick -> Bass Unmasking (60 - 150 Hz): Fast dynamic notch (2.0 - 4.0 dB) on Kick transient.
 *  2. Vocal -> Music Unmasking (1 - 4 kHz): Dynamic Mid-channel presence dip (2.0 - 3.0 dB) during vocal phrases.
 */
class CrossAdaptiveDucker {
public:
    enum class DuckingProfile : uint8_t {
        KickToBass = 0,
        VocalToMusic
    };

    void prepare(double sRate, DuckingProfile prof) noexcept {
        sampleRate = sRate;
        profile = prof;

        if (profile == DuckingProfile::KickToBass) {
            centerFreqHz = 85.0;
            filterQ = 1.3;
            maxCutDb = 3.5;
            thresholdDb = -24.0;
            // Fast 1 ms attack to catch kick onset, 80 ms release
            attackCoeff = std::exp(-1.0 / (0.001 * sampleRate));
            releaseCoeff = std::exp(-1.0 / (0.080 * sampleRate));
            // Sidechain bandpass to isolate kick thump (50 - 120 Hz)
            scBandPassL.setCoefficients(Biquad::makeBandPass(sampleRate, 85.0, 1.2));
            scBandPassR.setCoefficients(Biquad::makeBandPass(sampleRate, 85.0, 1.2));
            midChannelOnly = false;
        } else { // VocalToMusic
            centerFreqHz = 2500.0;
            filterQ = 1.0;
            maxCutDb = 2.5;
            thresholdDb = -26.0;
            // 5 ms attack, 220 ms musical release for smooth vocal phrasing
            attackCoeff = std::exp(-1.0 / (0.005 * sampleRate));
            releaseCoeff = std::exp(-1.0 / (0.220 * sampleRate));
            // Sidechain bandpass to isolate vocal formant presence (1 - 4 kHz)
            scBandPassL.setCoefficients(Biquad::makeBandPass(sampleRate, 2500.0, 1.0));
            scBandPassR.setCoefficients(Biquad::makeBandPass(sampleRate, 2500.0, 1.0));
            midChannelOnly = true;
        }

        dynEqFilterL.setCoefficients(Biquad::makePeakEQ(sampleRate, centerFreqHz, filterQ, 0.0));
        dynEqFilterR.setCoefficients(Biquad::makePeakEQ(sampleRate, centerFreqHz, filterQ, 0.0));
        reset();
    }

    void reset() noexcept {
        dynEqFilterL.reset();
        dynEqFilterR.reset();
        scBandPassL.reset();
        scBandPassR.reset();
        scEnvelope = 0.0;
        currentCutDb = 0.0f;
        maxCutSessionDb = 0.0f;
    }

    [[nodiscard]] float getCurrentCutDb() const noexcept { return currentCutDb; }
    [[nodiscard]] float getMaxCutDb() const noexcept { return maxCutSessionDb; }

    /**
     * Process main audio channels dynamically ducked by the provided sidechain key audio.
     */
    void processBlock(float* const* channels, int numChannels,
                      const float* const* sidechainChannels, int numScChannels,
                      int numSamples) noexcept {
        if (numSamples <= 0 || channels == nullptr) return;

        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        const float* scL = (sidechainChannels != nullptr && numScChannels > 0) ? sidechainChannels[0] : nullptr;
        const float* scR = (sidechainChannels != nullptr && numScChannels > 1) ? sidechainChannels[1] : scL;

        double blockMaxCut = 0.0;

        for (int i = 0; i < numSamples; ++i) {
            // 1. Detect sidechain energy in the keying band
            double scVal = 0.0;
            if (scL != nullptr) {
                const double inScL = static_cast<double>(scL[i]);
                const double inScR = (scR != nullptr) ? static_cast<double>(scR[i]) : inScL;
                const double filteredScL = scBandPassL.processSample(inScL);
                const double filteredScR = scBandPassR.processSample(inScR);
                scVal = std::max(std::abs(filteredScL), std::abs(filteredScR));
            }

            // Ballistic envelope detector
            if (scVal > scEnvelope) {
                scEnvelope = attackCoeff * scEnvelope + (1.0 - attackCoeff) * scVal;
            } else {
                scEnvelope = releaseCoeff * scEnvelope + (1.0 - releaseCoeff) * scVal;
            }

            // 2. Compute dynamic cut
            const double scDb = (scEnvelope > 1.0e-5) ? 20.0 * std::log10(scEnvelope) : -100.0;
            double cutDb = 0.0;
            if (scDb > thresholdDb) {
                const double over = scDb - thresholdDb;
                // Linear compression mapping up to maxCutDb
                cutDb = std::min(maxCutDb, over * 0.35);
            }

            if (cutDb > blockMaxCut) {
                blockMaxCut = cutDb;
            }

            // Update Dynamic EQ coefficients when cut changes significantly
            if (std::abs(cutDb - currentCutDb) > 0.05) {
                currentCutDb = static_cast<float>(cutDb);
                const auto eqCoeffs = Biquad::makePeakEQ(sampleRate, centerFreqHz, filterQ, -cutDb);
                dynEqFilterL.setCoefficients(eqCoeffs);
                dynEqFilterR.setCoefficients(eqCoeffs);
            }

            // 3. Apply dynamic filtering
            if (currentCutDb > 0.05f) {
                if (midChannelOnly && numChannels > 1) {
                    // M/S processing: apply presence dip to Mid channel only
                    const double inL = static_cast<double>(chL[i]);
                    const double inR = static_cast<double>(chR[i]);
                    const double mid = (inL + inR) * 0.5;
                    const double side = (inL - inR) * 0.5;

                    const double duckedMid = dynEqFilterL.processSample(mid);

                    chL[i] = static_cast<float>(duckedMid + side);
                    chR[i] = static_cast<float>(duckedMid - side);
                } else {
                    chL[i] = static_cast<float>(dynEqFilterL.processSample(static_cast<double>(chL[i])));
                    if (numChannels > 1) {
                        chR[i] = static_cast<float>(dynEqFilterR.processSample(static_cast<double>(chR[i])));
                    }
                }
            }
        }

        if (static_cast<float>(blockMaxCut) > maxCutSessionDb) {
            maxCutSessionDb = static_cast<float>(blockMaxCut);
        }
    }

private:
    double sampleRate = 48000.0;
    DuckingProfile profile = DuckingProfile::KickToBass;

    double centerFreqHz = 85.0;
    double filterQ = 1.3;
    double maxCutDb = 3.5;
    double thresholdDb = -24.0;

    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    double scEnvelope = 0.0;

    float currentCutDb = 0.0f;
    float maxCutSessionDb = 0.0f;
    bool midChannelOnly = false;

    Biquad dynEqFilterL, dynEqFilterR;
    Biquad scBandPassL, scBandPassR;
};

} // namespace dsp::mix
