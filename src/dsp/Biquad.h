#pragma once
#include <cmath>
#include <numbers>
#include <algorithm>

namespace dsp {

/**
 * Direct Form II Transposed Biquad Filter.
 * Canonical structure providing excellent numerical stability, zero denormals,
 * and minimal state memory (z1, z2) suitable for real-time sample processing.
 */
struct BiquadCoeffs {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

class Biquad {
public:
    Biquad() noexcept = default;

    void setCoefficients(const BiquadCoeffs& newCoeffs) noexcept {
        coeffs = newCoeffs;
    }

    void reset() noexcept {
        z1 = 0.0;
        z2 = 0.0;
    }

    [[nodiscard]] inline double processSample(double x) noexcept {
        // Direct Form II Transposed implementation:
        // y[n] = b0*x[n] + z1[n-1]
        // z1[n] = b1*x[n] - a1*y[n] + z2[n-1]
        // z2[n] = b2*x[n] - a2*y[n]
        const double y = coeffs.b0 * x + z1;
        z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
        z2 = coeffs.b2 * x - coeffs.a2 * y;
        return y;
    }

    [[nodiscard]] inline float processSample(float x) noexcept {
        return static_cast<float>(processSample(static_cast<double>(x)));
    }

    // Helper: 2nd-order Butterworth Low-Pass Filter
    static BiquadCoeffs makeLowPass(double sampleRate, double cutoffHz, double q = 0.7071067811865475) noexcept {
        const double omega = 2.0 * std::numbers::pi * cutoffHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * q);

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = ((1.0 - cs) * 0.5) / a0;
        c.b1 = (1.0 - cs) / a0;
        c.b2 = ((1.0 - cs) * 0.5) / a0;
        c.a1 = (-2.0 * cs) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    // Helper: 2nd-order Butterworth High-Pass Filter
    static BiquadCoeffs makeHighPass(double sampleRate, double cutoffHz, double q = 0.7071067811865475) noexcept {
        const double omega = 2.0 * std::numbers::pi * cutoffHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * q);

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = ((1.0 + cs) * 0.5) / a0;
        c.b1 = -(1.0 + cs) / a0;
        c.b2 = ((1.0 + cs) * 0.5) / a0;
        c.a1 = (-2.0 * cs) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    // Helper: RBJ Parametric Peak / Bell EQ
    static BiquadCoeffs makePeakEQ(double sampleRate, double freqHz, double q, double gainDb) noexcept {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * std::max(0.01, q));

        const double a0 = 1.0 + alpha / A;
        BiquadCoeffs c;
        c.b0 = (1.0 + alpha * A) / a0;
        c.b1 = (-2.0 * cs) / a0;
        c.b2 = (1.0 - alpha * A) / a0;
        c.a1 = (-2.0 * cs) / a0;
        c.a2 = (1.0 - alpha / A) / a0;
        return c;
    }

    // Helper: RBJ Low Shelf Filter
    static BiquadCoeffs makeLowShelf(double sampleRate, double freqHz, double q, double gainDb) noexcept {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * std::max(0.01, q));
        const double beta = 2.0 * std::sqrt(A) * alpha;

        const double a0 = (A + 1.0) + (A - 1.0) * cs + beta;
        BiquadCoeffs c;
        c.b0 = (A * ((A + 1.0) - (A - 1.0) * cs + beta)) / a0;
        c.b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cs)) / a0;
        c.b2 = (A * ((A + 1.0) - (A - 1.0) * cs - beta)) / a0;
        c.a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cs)) / a0;
        c.a2 = ((A + 1.0) + (A - 1.0) * cs - beta) / a0;
        return c;
    }

    // Helper: RBJ High Shelf Filter
    static BiquadCoeffs makeHighShelf(double sampleRate, double freqHz, double q, double gainDb) noexcept {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * std::max(0.01, q));
        const double beta = 2.0 * std::sqrt(A) * alpha;

        const double a0 = (A + 1.0) - (A - 1.0) * cs + beta;
        BiquadCoeffs c;
        c.b0 = (A * ((A + 1.0) + (A - 1.0) * cs + beta)) / a0;
        c.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cs)) / a0;
        c.b2 = (A * ((A + 1.0) + (A - 1.0) * cs - beta)) / a0;
        c.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cs)) / a0;
        c.a2 = ((A + 1.0) - (A - 1.0) * cs - beta) / a0;
        return c;
    }

    // Helper: RBJ Surgical Notch Filter
    static BiquadCoeffs makeNotch(double sampleRate, double freqHz, double q) noexcept {
        const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * std::max(0.01, q));

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = 1.0 / a0;
        c.b1 = (-2.0 * cs) / a0;
        c.b2 = 1.0 / a0;
        c.a1 = (-2.0 * cs) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    // Helper: RBJ Band-Pass Filter (constant 0 dB peak gain)
    static BiquadCoeffs makeBandPass(double sampleRate, double freqHz, double q) noexcept {
        const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
        const double sn = std::sin(omega);
        const double cs = std::cos(omega);
        const double alpha = sn / (2.0 * std::max(0.01, q));

        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = alpha / a0;
        c.b1 = 0.0;
        c.b2 = -alpha / a0;
        c.a1 = (-2.0 * cs) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

private:
    BiquadCoeffs coeffs;
    double z1 = 0.0;
    double z2 = 0.0;
};

/**
 * 4th-Order Linkwitz-Riley Filter Pair (Low-Pass and High-Pass).
 * Composed of two cascaded 2nd-order Butterworth filters (Q = 0.7071)
 * yielding -6 dB at crossover and perfectly flat summed magnitude response.
 */
class LinkwitzRiley4thOrder {
public:
    void prepare(double sampleRate, double crossoverHz) noexcept {
        const auto lpCoeffs = Biquad::makeLowPass(sampleRate, crossoverHz, 0.7071067811865475);
        const auto hpCoeffs = Biquad::makeHighPass(sampleRate, crossoverHz, 0.7071067811865475);
        lpStage1.setCoefficients(lpCoeffs);
        lpStage2.setCoefficients(lpCoeffs);
        hpStage1.setCoefficients(hpCoeffs);
        hpStage2.setCoefficients(hpCoeffs);
        reset();
    }

    void reset() noexcept {
        lpStage1.reset();
        lpStage2.reset();
        hpStage1.reset();
        hpStage2.reset();
    }

    inline void processSample(double input, double& lowOut, double& highOut) noexcept {
        lowOut = lpStage2.processSample(lpStage1.processSample(input));
        highOut = hpStage2.processSample(hpStage1.processSample(input));
    }

private:
    Biquad lpStage1, lpStage2;
    Biquad hpStage1, hpStage2;
};

} // namespace dsp
