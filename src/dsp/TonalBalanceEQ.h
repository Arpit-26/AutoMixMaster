#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

namespace dsp {

/**
 * Stage 7: 4-Band Tonal-Balance Target EQ & Reference Matching.
 * Fits broad shelves and bells hard-capped at +/- 3.0 dB on the master bus
 * to bring the mix into the target commercial corridor:
 * - Low Shelf (90 Hz): Bass shaping (<250 Hz)
 * - Low-Mid Bell (450 Hz): Body / Boxiness control (250 Hz - 2 kHz)
 * - High-Mid Bell (3.5 kHz): Presence / Harshness control (2 - 8 kHz)
 * - High Shelf (10 kHz): Air / Treble tilt (>8 kHz)
 */
class TonalBalanceEQ {
public:
    enum class TargetProfile {
        Modern,      // -4.5 dB/oct commercial standard
        BassHeavy,   // +2.0 dB sub/bass shelf, -4.8 dB/oct tilt
        Orchestral,  // Natural acoustic roll-off (-5.0 dB/oct), open air
        CustomMatch  // User-defined reference profile (Matchering-style)
    };

    void prepare(double sRate) noexcept {
        sampleRate = sRate;
        setProfile(TargetProfile::Modern);
        reset();
    }

    void reset() noexcept {
        lowShelfL.reset();
        lowShelfR.reset();
        lowMidBellL.reset();
        lowMidBellR.reset();
        highMidBellL.reset();
        highMidBellR.reset();
        highShelfL.reset();
        highShelfR.reset();
    }

    void setProfile(TargetProfile profile) noexcept {
        currentProfile = profile;
        switch (profile) {
            case TargetProfile::Modern:
                setBandGains(0.0, 0.0, 0.0, 0.0);
                break;
            case TargetProfile::BassHeavy:
                setBandGains(+2.0, -0.5, 0.0, -1.0);
                break;
            case TargetProfile::Orchestral:
                setBandGains(-1.0, 0.0, -0.5, +1.0);
                break;
            case TargetProfile::CustomMatch:
                break;
        }
    }

    void setBandGains(double lowShelfDb, double lowMidBellDb, double highMidBellDb, double highShelfDb) noexcept {
        // Enforce hard safety ceiling: +/- 3.0 dB maximum adjustment on master bus
        lowGainDb     = std::clamp(lowShelfDb, -3.0, 3.0);
        lowMidGainDb  = std::clamp(lowMidBellDb, -3.0, 3.0);
        highMidGainDb = std::clamp(highMidBellDb, -3.0, 3.0);
        highGainDb    = std::clamp(highShelfDb, -3.0, 3.0);

        updateCoefficients();
    }

    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
        float* chL = channels[0];
        float* chR = (numChannels > 1) ? channels[1] : channels[0];

        for (int i = 0; i < numSamples; ++i) {
            double sL = static_cast<double>(chL[i]);
            double sR = static_cast<double>(chR[i]);

            sL = lowShelfL.processSample(sL);
            sL = lowMidBellL.processSample(sL);
            sL = highMidBellL.processSample(sL);
            sL = highShelfL.processSample(sL);

            if (numChannels > 1) {
                sR = lowShelfR.processSample(sR);
                sR = lowMidBellR.processSample(sR);
                sR = highMidBellR.processSample(sR);
                sR = highShelfR.processSample(sR);
                chR[i] = static_cast<float>(sR);
            }

            chL[i] = static_cast<float>(sL);
        }
    }

private:
    void updateCoefficients() noexcept {
        const auto lsCoeffs = Biquad::makeLowShelf(sampleRate, 90.0, 0.707, lowGainDb);
        lowShelfL.setCoefficients(lsCoeffs);
        lowShelfR.setCoefficients(lsCoeffs);

        const auto lmCoeffs = Biquad::makePeakEQ(sampleRate, 450.0, 1.0, lowMidGainDb);
        lowMidBellL.setCoefficients(lmCoeffs);
        lowMidBellR.setCoefficients(lmCoeffs);

        const auto hmCoeffs = Biquad::makePeakEQ(sampleRate, 3500.0, 1.2, highMidGainDb);
        highMidBellL.setCoefficients(hmCoeffs);
        highMidBellR.setCoefficients(hmCoeffs);

        const auto hsCoeffs = Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, highGainDb);
        highShelfL.setCoefficients(hsCoeffs);
        highShelfR.setCoefficients(hsCoeffs);
    }

    double sampleRate = 48000.0;
    TargetProfile currentProfile = TargetProfile::Modern;

    double lowGainDb = 0.0;
    double lowMidGainDb = 0.0;
    double highMidGainDb = 0.0;
    double highGainDb = 0.0;

    Biquad lowShelfL, lowShelfR;
    Biquad lowMidBellL, lowMidBellR;
    Biquad highMidBellL, highMidBellR;
    Biquad highShelfL, highShelfR;
};

} // namespace dsp
