#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 1: Input Trim, Pre-Limit Guard & 20 Hz Subsonic HPF.
 * Measures incoming PLR and peak/RMS levels, normalizes to safe master headroom (-6 dBFS peak
 * or -18 dBFS RMS if pre-limited), and strips DC/subsonic energy below 20 Hz using a 4th-order filter.
 */
class InputStage {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        subsonicHpfL.prepare(sampleRate, 20.0);
        subsonicHpfR.prepare(sampleRate, 20.0);
        trimGainLinear = 1.0f;
        preLimitedDetected = false;
        reset();
    }

    void reset() noexcept {
        subsonicHpfL.reset();
        subsonicHpfR.reset();
        // Note: preserve trimGainLinear and preLimitedDetected across analyzer resets
    }

    void setTrimGainLinear(float gain) noexcept {
        trimGainLinear = gain;
    }

    void configureFromInputTelemetry(double inputPLR, double inputLUFS, float inputPeakLinear) noexcept {
        if (inputPLR <= 8.0 && inputLUFS >= -11.0) {
            // Mix is already smashed/pre-limited; trim input to conservative -18 dBFS RMS
            preLimitedDetected = true;
            const float targetRmsLinear = static_cast<float>(std::pow(10.0, -18.0 / 20.0));
            const float currentRmsLinear = static_cast<float>(std::pow(10.0, inputLUFS / 20.0));
            if (currentRmsLinear > 1e-5f) {
                trimGainLinear = targetRmsLinear / currentRmsLinear;
            }
        } else {
            preLimitedDetected = false;
            // Trim peaks to -6.0 dBFS (0.5012 linear) headroom
            constexpr float targetPeakLinear = 0.501187f; // -6.0 dBFS
            if (inputPeakLinear > targetPeakLinear && inputPeakLinear > 1e-4f) {
                trimGainLinear = targetPeakLinear / inputPeakLinear;
            } else if (inputPeakLinear < 0.1f && inputPeakLinear > 1e-5f) {
                // If excessively quiet (peaks below -20 dBFS), gently boost toward -6 dBFS
                trimGainLinear = std::min(4.0f, targetPeakLinear / inputPeakLinear);
            } else {
                trimGainLinear = 1.0f;
            }
        }
    }

    [[nodiscard]] bool isPreLimited() const noexcept { return preLimitedDetected; }
    [[nodiscard]] float getTrimGainLinear() const noexcept { return trimGainLinear; }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        for (int i = 0; i < numSamples; ++i) {
            const double sL = static_cast<double>(chL[i] * trimGainLinear);
            const double sR = static_cast<double>(chR[i] * trimGainLinear);

            double lowL = 0.0, highL = 0.0;
            double lowR = 0.0, highR = 0.0;

            // 4th-order Linkwitz-Riley at 20 Hz: highOut carries audio above 20 Hz
            subsonicHpfL.processSample(sL, lowL, highL);
            subsonicHpfR.processSample(sR, lowR, highR);

            chL[i] = static_cast<float>(highL);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(highR);
            }
        }
    }

private:
    double sampleRate = 48000.0;
    LinkwitzRiley4thOrder subsonicHpfL;
    LinkwitzRiley4thOrder subsonicHpfR;
    float trimGainLinear = 1.0f;
    bool preLimitedDetected = false;
};

} // namespace dsp
