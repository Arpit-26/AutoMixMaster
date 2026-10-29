#pragma once
#include "Biquad.h"
#include <cmath>
#include <numbers>

namespace dsp {

/**
 * Sample-Rate Adaptive ITU-R BS.1770-4 K-Weighting Filter.
 * Stage 1: Acoustic head-diffraction pre-filter (high-shelf, f0 = 1681.97 Hz, +4 dB).
 * Stage 2: Revised Low-frequency B-weighting (RLB) high-pass filter (f0 = 38.14 Hz).
 * Bilinear transform coefficients are computed analytically from sampleRate.
 */
class KWeightingFilter {
public:
    KWeightingFilter() noexcept = default;

    void prepare(double sampleRate) noexcept {
        currentSampleRate = sampleRate;

        // Stage 1: Acoustic Head-Diffraction High-Shelf Filter
        {
            constexpr double f0 = 1681.974450955533;
            constexpr double G  = 3.999843853973347;
            constexpr double Q  = 0.7071752369554196;

            const double K  = std::tan(std::numbers::pi * f0 / sampleRate);
            const double Vh = std::pow(10.0, G / 20.0);
            const double Vb = std::pow(Vh, 0.4996667741545416);
            const double a0 = 1.0 + (K / Q) + (K * K);

            BiquadCoeffs c;
            c.b0 = (Vh + Vb * (K / Q) + (K * K)) / a0;
            c.b1 = 2.0 * (K * K - Vh) / a0;
            c.b2 = (Vh - Vb * (K / Q) + (K * K)) / a0;
            c.a1 = 2.0 * (K * K - 1.0) / a0;
            c.a2 = (1.0 - (K / Q) + (K * K)) / a0;

            preFilter.setCoefficients(c);
            preFilter.reset();
        }

        // Stage 2: RLB High-Pass Filter (~38.14 Hz)
        {
            constexpr double f0 = 38.13547087602444;
            constexpr double Q  = 0.5003270373238773;

            const double K  = std::tan(std::numbers::pi * f0 / sampleRate);
            const double a0 = 1.0 + (K / Q) + (K * K);

            BiquadCoeffs c;
            c.b0 =  1.0 / a0;
            c.b1 = -2.0 / a0;
            c.b2 =  1.0 / a0;
            c.a1 =  2.0 * (K * K - 1.0) / a0;
            c.a2 = (1.0 - (K / Q) + (K * K)) / a0;

            rlbFilter.setCoefficients(c);
            rlbFilter.reset();
        }
    }

    void reset() noexcept {
        preFilter.reset();
        rlbFilter.reset();
    }

    [[nodiscard]] inline double processSample(double x) noexcept {
        return rlbFilter.processSample(preFilter.processSample(x));
    }

    [[nodiscard]] inline float processSample(float x) noexcept {
        return static_cast<float>(processSample(static_cast<double>(x)));
    }

    [[nodiscard]] double getSampleRate() const noexcept {
        return currentSampleRate;
    }

private:
    double currentSampleRate = 48000.0;
    Biquad preFilter;
    Biquad rlbFilter;
};

} // namespace dsp
