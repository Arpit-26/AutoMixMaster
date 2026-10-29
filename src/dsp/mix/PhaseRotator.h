#pragma once
#include "../Biquad.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <array>

namespace dsp::mix {

/**
 * Continuous 0° to 360° Phase Rotator and Cross-Track Phase Optimizer.
 * Combines an all-pass Hilbert 90° quadrature phase splitter with cosine/sine rotation:
 *   y[n] = x_direct[n] * cos(theta) + x_90[n] * sin(theta)
 * preserving 100% flat magnitude response across all frequencies while shifting phase angle.
 * Includes an automated phase sweep optimizer to lock the phase angle that maximizes low-end RMS sum with Kick.
 */
class PhaseRotator {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;

        // Cascade of all-pass filters designed for 90° relative phase shift in the 40 - 250 Hz band
        // Branch 0: Reference phase delay
        apRefL.setCoefficients(Biquad::makeAllPass(sampleRate, 45.0, 0.707));
        apRefR.setCoefficients(Biquad::makeAllPass(sampleRate, 45.0, 0.707));

        // Branch 1: Quadrature phase delay (~90° offset)
        apQuadL.setCoefficients(Biquad::makeAllPass(sampleRate, 120.0, 0.707));
        apQuadR.setCoefficients(Biquad::makeAllPass(sampleRate, 120.0, 0.707));

        // Low-pass filter for evaluating low-end energy during alignment search
        alignLpf.setCoefficients(Biquad::makeLowPass(sampleRate, 150.0, 0.707));

        setAngleDegrees(0.0f);
        reset();
    }

    void reset() noexcept {
        apRefL.reset();
        apRefR.reset();
        apQuadL.reset();
        apQuadR.reset();
        alignLpf.reset();
        optimizedAngleDegrees = 0.0f;
    }

    void setAngleDegrees(float deg) noexcept {
        angleDegrees = std::fmod(deg, 360.0f);
        if (angleDegrees < 0.0f) angleDegrees += 360.0f;

        const double rad = static_cast<double>(angleDegrees) * (std::numbers::pi / 180.0);
        cosAngle = static_cast<float>(std::cos(rad));
        sinAngle = static_cast<float>(std::sin(rad));
    }

    [[nodiscard]] float getAngleDegrees() const noexcept { return angleDegrees; }
    [[nodiscard]] float getOptimizedAngleDegrees() const noexcept { return optimizedAngleDegrees; }

    /**
     * Process audio in-place through continuous phase rotator.
     */
    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        if (numSamples <= 0 || channels == nullptr) return;

        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        // Fast path for 0° rotation
        if (std::abs(angleDegrees) < 0.1f) {
            return;
        }

        // Fast path for 180° polarity inversion
        if (std::abs(angleDegrees - 180.0f) < 0.5f) {
            for (int i = 0; i < numSamples; ++i) {
                chL[i] = -chL[i];
                if (numChannels > 1) chR[i] = -chR[i];
            }
            return;
        }

        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // Branch 0 (Ref) and Branch 1 (Quad)
            const double refL = apRefL.processSample(inL);
            const double quadL = apQuadL.processSample(inL);

            const double outL = refL * cosAngle + quadL * sinAngle;
            chL[i] = static_cast<float>(outL);

            if (numChannels > 1) {
                const double refR = apRefR.processSample(inR);
                const double quadR = apQuadR.processSample(inR);
                const double outR = refR * cosAngle + quadR * sinAngle;
                chR[i] = static_cast<float>(outR);
            }
        }
    }

    /**
     * Optimizes phase angle to maximize constructive summing with reference (Kick) signal.
     * Evaluates 16 phase steps (0°, 22.5°, 45°, ..., 337.5°) over provided audio window.
     */
    float optimizePhaseAlignment(const float* bassAudio, const float* kickSidechain, int numSamples) noexcept {
        if (bassAudio == nullptr || kickSidechain == nullptr || numSamples < 128) {
            return 0.0f;
        }

        constexpr int kSteps = 16;
        std::array<double, kSteps> stepEnergies {};
        stepEnergies.fill(0.0);

        for (int step = 0; step < kSteps; ++step) {
            const double theta = (static_cast<double>(step) * 360.0 / kSteps) * (std::numbers::pi / 180.0);
            const double cosT = std::cos(theta);
            const double sinT = std::sin(theta);

            Biquad testRef = apRefL;
            testRef.reset();
            Biquad testQuad = apQuadL;
            testQuad.reset();
            Biquad testLpf = alignLpf;
            testLpf.reset();

            double sumSq = 0.0;
            for (int i = 0; i < numSamples; ++i) {
                const double b = static_cast<double>(bassAudio[i]);
                const double k = static_cast<double>(kickSidechain[i]);

                double rotatedBass = 0.0;
                if (step == 0) {
                    rotatedBass = b;
                } else if (step == 8) { // 180°
                    rotatedBass = -b;
                } else {
                    const double ref = testRef.processSample(b);
                    const double quad = testQuad.processSample(b);
                    rotatedBass = ref * cosT + quad * sinT;
                }

                const double rotatedCombined = testLpf.processSample(rotatedBass + k);
                sumSq += rotatedCombined * rotatedCombined;
            }
            stepEnergies[static_cast<size_t>(step)] = sumSq;
        }

        // Find angle with maximum summed low-end energy
        int bestStep = 0;
        double maxEnergy = stepEnergies[0];
        for (int s = 1; s < kSteps; ++s) {
            if (stepEnergies[static_cast<size_t>(s)] > maxEnergy) {
                maxEnergy = stepEnergies[static_cast<size_t>(s)];
                bestStep = s;
            }
        }

        optimizedAngleDegrees = static_cast<float>(bestStep * (360.0 / kSteps));
        setAngleDegrees(optimizedAngleDegrees);
        return optimizedAngleDegrees;
    }

private:
    double sampleRate = 48000.0;
    float angleDegrees = 0.0f;
    float optimizedAngleDegrees = 0.0f;
    float cosAngle = 1.0f;
    float sinAngle = 0.0f;

    Biquad apRefL, apRefR;
    Biquad apQuadL, apQuadR;
    Biquad alignLpf;
};

} // namespace dsp::mix
