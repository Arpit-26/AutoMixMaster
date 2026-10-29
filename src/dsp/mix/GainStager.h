#pragma once
#include <algorithm>
#include <cmath>

namespace dsp::mix {

/**
 * Automatic Input Gain Staging normalizer.
 * Automatically aligns incoming stem signals to -18.0 dBFS RMS (analog 0 VU reference)
 * prior to dynamics and saturation processing with click-free parameter smoothing.
 */
class GainStager {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        targetRmsDbFS = -18.0f;
        // 200 ms smoothing time constant to gracefully track dynamics without zipper noise
        smoothingCoeff = std::exp(-1.0 / (0.200 * sampleRate));
        reset();
    }

    void reset() noexcept {
        currentRmsLinear = 0.0f;
        targetGainLinear = 1.0f;
        smoothedGainLinear = 1.0f;
        sumSquared = 0.0;
        sampleCount = 0;
        measuredInputRmsDbFS = -70.0f;
        measuredPostRmsDbFS = -70.0f;
        isFirstUpdate = true;
        calibrated = false;
    }

    void calibrateFromBuffer(const float* const* channels, int numChannels, int numSamples) noexcept {
        if (numSamples <= 0 || channels == nullptr || numChannels <= 0) return;
        double sumSq = 0.0;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float* p = channels[ch];
            for (int i = 0; i < numSamples; ++i) {
                const double s = static_cast<double>(p[i]);
                sumSq += s * s;
            }
        }
        const double meanSq = sumSq / static_cast<double>(numChannels * numSamples);
        const float rms = static_cast<float>(std::sqrt(std::max(1.0e-9, meanSq)));
        currentRmsLinear = rms;
        measuredInputRmsDbFS = (rms > 1.0e-5f) ? 20.0f * std::log10(rms) : -70.0f;

        const float targetLin = std::pow(10.0f, targetRmsDbFS / 20.0f);
        targetGainLinear = targetLin / std::max(1.0e-5f, rms);
        targetGainLinear = std::clamp(targetGainLinear, 0.0316f, 15.85f);
        smoothedGainLinear = targetGainLinear;
        isFirstUpdate = false;
        calibrated = true;
    }

    void setTargetRmsDbFS(float targetDb) noexcept {
        targetRmsDbFS = targetDb;
    }

    [[nodiscard]] float getTargetRmsDbFS() const noexcept { return targetRmsDbFS; }
    [[nodiscard]] float getInputRmsDbFS() const noexcept { return measuredInputRmsDbFS; }
    [[nodiscard]] float getPostGainRmsDbFS() const noexcept { return measuredPostRmsDbFS; }
    [[nodiscard]] float getGainStagingDb() const noexcept {
        return (smoothedGainLinear > 1.0e-5f) ? 20.0f * std::log10(smoothedGainLinear) : 0.0f;
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        if (numSamples <= 0 || channels == nullptr) return;

        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        if (calibrated) {
            for (int i = 0; i < numSamples; ++i) {
                chL[i] *= smoothedGainLinear;
                if (numChannels > 1) chR[i] *= smoothedGainLinear;
            }
            measuredPostRmsDbFS = targetRmsDbFS;
            return;
        }

        // 1. Accumulate RMS across block
        double blockSumSq = 0.0;
        for (int i = 0; i < numSamples; ++i) {
            const double sL = static_cast<double>(chL[i]);
            const double sR = static_cast<double>(chR[i]);
            blockSumSq += (sL * sL + sR * sR) * 0.5;
        }

        sumSquared += blockSumSq;
        sampleCount += numSamples;

        // Update RMS estimate every ~50ms
        const int updateInterval = std::max(128, static_cast<int>(sampleRate * 0.05));
        if (sampleCount >= updateInterval) {
            const double meanSq = sumSquared / static_cast<double>(sampleCount);
            const float rms = static_cast<float>(std::sqrt(std::max(0.0, meanSq)));
            currentRmsLinear = rms;
            measuredInputRmsDbFS = (rms > 1.0e-5f) ? 20.0f * std::log10(rms) : -70.0f;

            // Only adapt gain during active signal (> -45 dBFS) so silence between hits does not cause runaway boost
            if (measuredInputRmsDbFS > -45.0f) {
                const float targetLin = std::pow(10.0f, targetRmsDbFS / 20.0f);
                targetGainLinear = targetLin / std::max(1.0e-5f, rms);
                targetGainLinear = std::clamp(targetGainLinear, 0.0316f, 15.85f);

                if (isFirstUpdate) {
                    smoothedGainLinear = targetGainLinear;
                    isFirstUpdate = false;
                }
            }

            sumSquared = 0.0;
            sampleCount = 0;
        }

        // 2. Apply smoothed gain in-place
        double postSumSq = 0.0;
        for (int i = 0; i < numSamples; ++i) {
            smoothedGainLinear = static_cast<float>(smoothingCoeff * smoothedGainLinear + (1.0 - smoothingCoeff) * targetGainLinear);
            chL[i] *= smoothedGainLinear;
            if (numChannels > 1) {
                chR[i] *= smoothedGainLinear;
            }
            const double pL = static_cast<double>(chL[i]);
            const double pR = static_cast<double>(chR[i]);
            postSumSq += (pL * pL + pR * pR) * 0.5;
        }

        const float postRms = static_cast<float>(std::sqrt(std::max(0.0, postSumSq / numSamples)));
        measuredPostRmsDbFS = (postRms > 1.0e-5f) ? 20.0f * std::log10(postRms) : -70.0f;
    }

private:
    double sampleRate = 48000.0;
    double smoothingCoeff = 0.0;
    float targetRmsDbFS = -18.0f;

    float currentRmsLinear = 0.0f;
    float targetGainLinear = 1.0f;
    float smoothedGainLinear = 1.0f;
    bool isFirstUpdate = true;
    bool calibrated = false;

    double sumSquared = 0.0;
    int sampleCount = 0;
    float measuredInputRmsDbFS = -70.0f;
    float measuredPostRmsDbFS = -70.0f;
};

} // namespace dsp::mix
