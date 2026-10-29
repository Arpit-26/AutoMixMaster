#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp {

/**
 * Stage 9: Oversampled Soft Clipper.
 * Shaves 1.0 - 2.0 dB off percussive transient spikes using a smooth soft-knee transfer function,
 * relieving stress from the brickwall true-peak limiter and preventing pumping artifacts.
 * Operates with 4x internal oversampling to eliminate harmonic aliasing in the audible spectrum.
 */
class OversampledSoftClipper {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        // 4x oversampling half-band lowpass filters (Butterworth 2nd order at 0.22 fs)
        osFilterInL1.setCoefficients(Biquad::makeLowPass(sampleRate * 2.0, sampleRate * 0.45));
        osFilterInR1.setCoefficients(Biquad::makeLowPass(sampleRate * 2.0, sampleRate * 0.45));
        osFilterInL2.setCoefficients(Biquad::makeLowPass(sampleRate * 4.0, sampleRate * 0.45));
        osFilterInR2.setCoefficients(Biquad::makeLowPass(sampleRate * 4.0, sampleRate * 0.45));

        osFilterOutL1.setCoefficients(Biquad::makeLowPass(sampleRate * 4.0, sampleRate * 0.45));
        osFilterOutR1.setCoefficients(Biquad::makeLowPass(sampleRate * 4.0, sampleRate * 0.45));
        osFilterOutL2.setCoefficients(Biquad::makeLowPass(sampleRate * 2.0, sampleRate * 0.45));
        osFilterOutR2.setCoefficients(Biquad::makeLowPass(sampleRate * 2.0, sampleRate * 0.45));

        setThresholdDb(-2.0); // Default: start soft-clipping above -2.0 dBFS
        reset();
    }

    void reset() noexcept {
        osFilterInL1.reset();
        osFilterInR1.reset();
        osFilterInL2.reset();
        osFilterInR2.reset();
        osFilterOutL1.reset();
        osFilterOutR1.reset();
        osFilterOutL2.reset();
        osFilterOutR2.reset();
        peakShaved_dB = 0.0f;
        maxShaveSession = 0.0f;
    }

    void setThresholdDb(double threshDb) noexcept {
        thresholdDb = threshDb;
        thresholdLinear = static_cast<float>(std::pow(10.0, thresholdDb / 20.0));
    }

    [[nodiscard]] float getPeakShaved_dB() const noexcept {
        return peakShaved_dB;
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        float maxShaveBlock = 0.0f;

        for (int i = 0; i < numSamples; ++i) {
            const float inL = chL[i];
            const float inR = chR[i];

            // 1. 4x Upsampling with zero-stuffing and anti-imaging filtering
            // Step 1: 1x -> 2x
            const double up2L_0 = osFilterInL1.processSample(static_cast<double>(inL) * 2.0);
            const double up2L_1 = osFilterInL1.processSample(0.0);
            const double up2R_0 = osFilterInR1.processSample(static_cast<double>(inR) * 2.0);
            const double up2R_1 = osFilterInR1.processSample(0.0);

            // Step 2: 2x -> 4x
            const double up4L_0 = osFilterInL2.processSample(up2L_0 * 2.0);
            const double up4L_1 = osFilterInL2.processSample(0.0);
            const double up4L_2 = osFilterInL2.processSample(up2L_1 * 2.0);
            const double up4L_3 = osFilterInL2.processSample(0.0);

            const double up4R_0 = osFilterInR2.processSample(up2R_0 * 2.0);
            const double up4R_1 = osFilterInR2.processSample(0.0);
            const double up4R_2 = osFilterInR2.processSample(up2R_1 * 2.0);
            const double up4R_3 = osFilterInR2.processSample(0.0);

            // 2. Soft-clip transfer function at 4x rate
            double clpL0 = clipSample(up4L_0, maxShaveBlock);
            double clpL1 = clipSample(up4L_1, maxShaveBlock);
            double clpL2 = clipSample(up4L_2, maxShaveBlock);
            double clpL3 = clipSample(up4L_3, maxShaveBlock);

            double clpR0 = clipSample(up4R_0, maxShaveBlock);
            double clpR1 = clipSample(up4R_1, maxShaveBlock);
            double clpR2 = clipSample(up4R_2, maxShaveBlock);
            double clpR3 = clipSample(up4R_3, maxShaveBlock);

            // 3. 4x Decimation with anti-aliasing filtering
            // Filter at 4x
            (void) osFilterOutL1.processSample(clpL0);
            (void) osFilterOutL1.processSample(clpL1);
            const double dn2L_0 = osFilterOutL1.processSample(clpL2);
            (void) osFilterOutL1.processSample(clpL3);

            (void) osFilterOutR1.processSample(clpR0);
            (void) osFilterOutR1.processSample(clpR1);
            const double dn2R_0 = osFilterOutR1.processSample(clpR2);
            (void) osFilterOutR1.processSample(clpR3);

            // Filter at 2x and downsample to 1x
            const double outL = osFilterOutL2.processSample(dn2L_0);
            const double outR = osFilterOutR2.processSample(dn2R_0);

            chL[i] = static_cast<float>(outL);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(outR);
            }
        }

        if (maxShaveBlock > maxShaveSession) {
            maxShaveSession = maxShaveBlock;
        }
        peakShaved_dB = maxShaveSession;
    }

private:
    float maxShaveSession = 0.0f;
    [[nodiscard]] inline double clipSample(double x, float& maxShave) const noexcept {
        const double absX = std::abs(x);
        const double T = static_cast<double>(thresholdLinear);
        if (absX <= T) {
            return x;
        }

        // Smooth cubic/tanh soft saturation
        const double diff = absX - T;
        const double margin = 1.0 - T;
        const double sat = T + margin * std::tanh(diff / std::max(0.01, margin));

        // Strict 2.0 dB transient reduction cap:
        // Capped so the soft clipper never shaves more than 2.0 dB off any transient
        constexpr double kMinGainLinear = 0.7943282347242815; // 10^(-2.0 / 20.0) = -2.0 dB
        const double minAllowedSat = absX * kMinGainLinear;
        const double cappedSat = std::max(sat, minAllowedSat);

        const double y = (x >= 0.0) ? cappedSat : -cappedSat;

        const float shaveDb = static_cast<float>(20.0 * std::log10(absX / std::max(1e-6, cappedSat)));
        if (shaveDb > maxShave) {
            maxShave = shaveDb;
        }
        return y;
    }

    double sampleRate = 48000.0;
    double thresholdDb = -2.0;
    float thresholdLinear = 0.794328f;

    Biquad osFilterInL1, osFilterInR1;
    Biquad osFilterInL2, osFilterInR2;
    Biquad osFilterOutL1, osFilterOutR1;
    Biquad osFilterOutL2, osFilterOutR2;

    float peakShaved_dB = 0.0f;
};

} // namespace dsp
