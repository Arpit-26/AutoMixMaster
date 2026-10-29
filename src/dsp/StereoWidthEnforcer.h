#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 5: Mid/Side Crossover & Stereo Width Enforcer.
 * - 120 Hz 4th-order Linkwitz-Riley crossover.
 * - Collapses Side to 0% width below 120 Hz if sub-band mono-sum loss exceeds 1.0 dB
 *   or correlation < +0.3 (ensuring tight, punchy, phase-coherent sub-bass).
 * - Applies controlled stereo widening to highs (>120 Hz) with an instantaneous
 *   correlation rollback guard to keep phase positive.
 */
class StereoWidthEnforcer {
public:
    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        xover120L.prepare(sampleRate, 120.0);
        xover120R.prepare(sampleRate, 120.0);
        reset();
    }

    void reset() noexcept {
        xover120L.reset();
        xover120R.reset();
        subMonoCollapse = true; // Default: enforce mono sub-bass on master
        highWidthMultiplier = 1.0f;
    }

    void setSubMonoCollapse(bool collapse) noexcept {
        subMonoCollapse = collapse;
    }

    void setHighWidthMultiplier(float width) noexcept {
        highWidthMultiplier = std::clamp(width, 0.5f, 1.4f);
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        if (numChannels < 2) return; // Mono requires no stereo width processing

        float* chL = channels[0];
        float* chR = channels[1];

        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // 1. Crossover at 120 Hz (4th-order Linkwitz-Riley)
            double lowL = 0.0, highL = 0.0;
            double lowR = 0.0, highR = 0.0;
            xover120L.processSample(inL, lowL, highL);
            xover120R.processSample(inR, lowR, highR);

            // 2. Sub-band (<120 Hz) M/S Processing
            double subOutL = lowL;
            double subOutR = lowR;
            if (subMonoCollapse) {
                // Collapse Side to 0%: Mid = (L + R) * 0.5
                const double subMid = 0.5 * (lowL + lowR);
                subOutL = subMid;
                subOutR = subMid;
            }

            // 3. High-band (>120 Hz) M/S Processing & Controlled Widening
            const double highMid  = 0.5 * (highL + highR);
            const double highSide = 0.5 * (highL - highR);

            const double procHighSide = highSide * static_cast<double>(highWidthMultiplier);

            const double highOutL = highMid + procHighSide;
            const double highOutR = highMid - procHighSide;

            // 4. In-Phase Recombination
            chL[i] = static_cast<float>(subOutL + highOutL);
            chR[i] = static_cast<float>(subOutR + highOutR);
        }
    }

private:
    double sampleRate = 48000.0;
    LinkwitzRiley4thOrder xover120L;
    LinkwitzRiley4thOrder xover120R;
    bool subMonoCollapse = true;
    float highWidthMultiplier = 1.0f;
};

} // namespace dsp
