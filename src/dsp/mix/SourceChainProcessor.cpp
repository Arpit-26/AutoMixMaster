#include "SourceChainProcessor.h"
#include <algorithm>
#include <cmath>

namespace dsp::mix {

void SourceChainProcessor::prepare(double sRate, TrackClass trackClass) noexcept {
    sampleRate = sRate;
    currentClass = trackClass;

    gainStager.prepare(sampleRate);
    phaseRotator.prepare(sampleRate);
    saturator.prepare(sampleRate);
    subXoverL.prepare(sampleRate, 120.0);
    subXoverR.prepare(sampleRate, 120.0);

    setupChainForClass();
    reset();
}

void SourceChainProcessor::reset() noexcept {
    gainStager.reset();
    phaseRotator.reset();
    crossDucker.reset();
    saturator.reset();
    subXoverL.reset();
    subXoverR.reset();

    hpfL.reset();
    hpfR.reset();
    mudCutL.reset();
    mudCutR.reset();
    harshNotchL.reset();
    harshNotchR.reset();
    boostLowL.reset();
    boostLowR.reset();
    boostHighL.reset();
    boostHighR.reset();

    deEsserBandpassL.reset();
    deEsserBandpassR.reset();
    deEsserFilterL.reset();
    deEsserFilterR.reset();

    compEnvelope = 1.0;
    currentCompGrDb = 0.0f;
    maxCompGrSessionDb = 0.0f;

    deEsserEnvelope = 0.0;
    currentDeEsserGrDb = 0.0f;
    maxDeEsserSessionDb = 0.0f;
}

void SourceChainProcessor::setTrackClass(TrackClass newClass) noexcept {
    if (currentClass != newClass) {
        currentClass = newClass;
        setupChainForClass();
        reset();
    }
}

void SourceChainProcessor::setSidechainKey(const float* const* scChannels, int numScChannels) noexcept {
    sidechainPointers = scChannels;
    sidechainChannelCount = numScChannels;
}

void SourceChainProcessor::optimizeLowEndPhase(const float* bassAudio, const float* kickAudio, int numSamples) noexcept {
    if (hasPhaseRotator && bassAudio != nullptr && kickAudio != nullptr && numSamples > 0) {
        phaseRotator.optimizePhaseAlignment(bassAudio, kickAudio, numSamples);
    }
}

void SourceChainProcessor::optimizeLowEndPhase(const float* kickAudio, int numSamples) noexcept {
    (void)kickAudio;
    (void)numSamples;
}

void SourceChainProcessor::setupChainForClass() noexcept {
    hasHpf = true;
    hasMudCut = false;
    hasHarshNotch = false;
    hasCompression = true;
    hasDeEsser = false;
    hasCrossDucking = false;
    hasPhaseRotator = false;
    hasSaturation = false;
    hasAdditiveEQ = false;
    forceMono = false;
    subMonoCollapse = false;

    double hpfFreq = 30.0;
    double compAttackMs = 15.0;
    double compReleaseMs = 100.0;
    compRatio = 3.0;
    compThresholdDb = -18.0;
    double makeupDb = 0.0;

    switch (currentClass) {
        case TrackClass::LeadVocal:
            hpfFreq = 100.0;
            makeupDb = 3.8;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 300.0, 1.5, -2.5));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 300.0, 1.5, -2.5));
            hasHarshNotch = true;
            harshNotchL.setCoefficients(Biquad::makePeakEQ(sampleRate, 3500.0, 3.5, -2.0));
            harshNotchR.setCoefficients(Biquad::makePeakEQ(sampleRate, 3500.0, 3.5, -2.0));
            compRatio = 3.0;
            compAttackMs = 12.0; // Consonant transient passing
            compReleaseMs = 120.0;
            compThresholdDb = -19.0;
            hasDeEsser = true;
            deEsserBandpassL.setCoefficients(Biquad::makeBandPass(sampleRate, 7200.0, 2.0));
            deEsserBandpassR.setCoefficients(Biquad::makeBandPass(sampleRate, 7200.0, 2.0));
            hasAdditiveEQ = true;
            boostHighL.setCoefficients(Biquad::makeHighShelf(sampleRate, 12000.0, 0.707, 1.5));
            boostHighR.setCoefficients(Biquad::makeHighShelf(sampleRate, 12000.0, 0.707, 1.5));
            hasSaturation = true;
            saturator.setDrive(1.2f);
            break;

        case TrackClass::BackingVocal:
            hpfFreq = 140.0;
            makeupDb = 2.0;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 350.0, 1.5, -3.0));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 350.0, 1.5, -3.0));
            hasHarshNotch = true;
            harshNotchL.setCoefficients(Biquad::makePeakEQ(sampleRate, 2500.0, 2.0, -2.5)); // Clear space for lead
            harshNotchR.setCoefficients(Biquad::makePeakEQ(sampleRate, 2500.0, 2.0, -2.5));
            compRatio = 4.0;
            compAttackMs = 8.0;
            compReleaseMs = 80.0;
            compThresholdDb = -18.0;
            hasAdditiveEQ = true;
            boostHighL.setCoefficients(Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, -1.0)); // Darker top
            boostHighR.setCoefficients(Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, -1.0));
            break;

        case TrackClass::Kick:
            hpfFreq = 28.0;
            makeupDb = 1.9;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 250.0, 2.0, -3.0));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 250.0, 2.0, -3.0));
            compRatio = 4.0;
            compAttackMs = 12.0; // Preserves transient crack
            compReleaseMs = 80.0;
            compThresholdDb = -16.0;
            hasAdditiveEQ = true;
            boostLowL.setCoefficients(Biquad::makePeakEQ(sampleRate, 60.0, 1.4, 2.0)); // Punch weight
            boostLowR.setCoefficients(Biquad::makePeakEQ(sampleRate, 60.0, 1.4, 2.0));
            boostHighL.setCoefficients(Biquad::makePeakEQ(sampleRate, 4000.0, 2.0, 1.5)); // Click snap
            boostHighR.setCoefficients(Biquad::makePeakEQ(sampleRate, 4000.0, 2.0, 1.5));
            forceMono = true;
            break;

        case TrackClass::Snare:
            hpfFreq = 100.0;
            makeupDb = 1.2;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 500.0, 2.0, -2.5));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 500.0, 2.0, -2.5));
            compRatio = 4.0;
            compAttackMs = 10.0; // Preserves crack
            compReleaseMs = 100.0;
            compThresholdDb = -17.0;
            hasAdditiveEQ = true;
            boostLowL.setCoefficients(Biquad::makePeakEQ(sampleRate, 200.0, 1.5, 2.0)); // Body weight
            boostLowR.setCoefficients(Biquad::makePeakEQ(sampleRate, 200.0, 1.5, 2.0));
            boostHighL.setCoefficients(Biquad::makePeakEQ(sampleRate, 5000.0, 1.8, 2.0)); // Snap
            boostHighR.setCoefficients(Biquad::makePeakEQ(sampleRate, 5000.0, 1.8, 2.0));
            break;

        case TrackClass::Sub808:
            hpfFreq = 28.0;
            makeupDb = 3.4;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 250.0, 2.0, -3.0));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 250.0, 2.0, -3.0));
            hasPhaseRotator = true;
            hasCrossDucking = true;
            crossDucker.prepare(sampleRate, CrossAdaptiveDucker::DuckingProfile::KickToBass);
            compRatio = 3.5;
            compAttackMs = 15.0;
            compReleaseMs = 90.0;
            compThresholdDb = -18.0;
            hasSaturation = true; // Harmonics for mobile phone translation
            saturator.setDrive(1.6f);
            forceMono = true;
            subMonoCollapse = false;
            break;

        case TrackClass::Bass:
            hpfFreq = 35.0;
            makeupDb = 1.8;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 350.0, 1.8, -2.5));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 350.0, 1.8, -2.5));
            hasPhaseRotator = true;
            hasCrossDucking = true;
            crossDucker.prepare(sampleRate, CrossAdaptiveDucker::DuckingProfile::KickToBass);
            compRatio = 4.0;
            compAttackMs = 10.0;
            compReleaseMs = 100.0;
            compThresholdDb = -18.0;
            hasAdditiveEQ = true;
            boostLowL.setCoefficients(Biquad::makePeakEQ(sampleRate, 80.0, 1.4, 1.5));
            boostLowR.setCoefficients(Biquad::makePeakEQ(sampleRate, 80.0, 1.4, 1.5));
            boostHighL.setCoefficients(Biquad::makePeakEQ(sampleRate, 900.0, 1.8, 2.0)); // Pick articulation
            boostHighR.setCoefficients(Biquad::makePeakEQ(sampleRate, 900.0, 1.8, 2.0));
            hasSaturation = true;
            saturator.setDrive(1.3f);
            forceMono = true;
            subMonoCollapse = false;
            break;

        case TrackClass::HiHats:
            hpfFreq = 400.0;
            makeupDb = 1.0;
            hasHarshNotch = true;
            harshNotchL.setCoefficients(Biquad::makePeakEQ(sampleRate, 4500.0, 3.0, -2.0));
            harshNotchR.setCoefficients(Biquad::makePeakEQ(sampleRate, 4500.0, 3.0, -2.0));
            compRatio = 4.0;
            compAttackMs = 2.0; // Fast transient taming
            compReleaseMs = 50.0;
            compThresholdDb = -16.0;
            hasAdditiveEQ = true;
            boostHighL.setCoefficients(Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, 2.0)); // Shimmer
            boostHighR.setCoefficients(Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, 2.0));
            break;

        case TrackClass::PadsKeysGuitars:
            hpfFreq = 120.0;
            makeupDb = 2.5;
            hasMudCut = true;
            mudCutL.setCoefficients(Biquad::makePeakEQ(sampleRate, 350.0, 1.5, -2.0));
            mudCutR.setCoefficients(Biquad::makePeakEQ(sampleRate, 350.0, 1.5, -2.0));
            hasCrossDucking = true;
            crossDucker.prepare(sampleRate, CrossAdaptiveDucker::DuckingProfile::VocalToMusic);
            compRatio = 2.5;
            compAttackMs = 20.0;
            compReleaseMs = 150.0;
            compThresholdDb = -18.0;
            break;

        case TrackClass::LeadSynth:
            hpfFreq = 100.0;
            makeupDb = 1.0;
            hasHarshNotch = true;
            harshNotchL.setCoefficients(Biquad::makePeakEQ(sampleRate, 3500.0, 3.5, -2.5));
            harshNotchR.setCoefficients(Biquad::makePeakEQ(sampleRate, 3500.0, 3.5, -2.5));
            compRatio = 3.0;
            compAttackMs = 15.0;
            compReleaseMs = 100.0;
            compThresholdDb = -18.0;
            hasAdditiveEQ = true;
            boostHighL.setCoefficients(Biquad::makePeakEQ(sampleRate, 3000.0, 1.5, 1.5));
            boostHighR.setCoefficients(Biquad::makePeakEQ(sampleRate, 3000.0, 1.5, 1.5));
            break;

        case TrackClass::DrumBus:
            hpfFreq = 25.0;
            makeupDb = 1.0;
            compRatio = 2.0;
            compAttackMs = 10.0;
            compReleaseMs = 100.0;
            compThresholdDb = -16.0;
            hasAdditiveEQ = true;
            boostHighL.setCoefficients(Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, 1.0));
            boostHighR.setCoefficients(Biquad::makeHighShelf(sampleRate, 10000.0, 0.707, 1.0));
            break;

        case TrackClass::MixBus:
        case TrackClass::Unknown:
        default:
            hpfFreq = 20.0;
            makeupDb = 0.0;
            compRatio = 1.75;
            compAttackMs = 25.0;
            compReleaseMs = 150.0;
            compThresholdDb = -18.0;
            subMonoCollapse = true;
            break;
    }

    outputMakeupGainLinear = static_cast<float>(std::pow(10.0, makeupDb / 20.0));

    // Configure HPF
    hpfL.setCoefficients(Biquad::makeHighPass(sampleRate, hpfFreq, 0.707));
    hpfR.setCoefficients(Biquad::makeHighPass(sampleRate, hpfFreq, 0.707));

    // Configure Compressor Ballistics
    compAttackCoeff = std::exp(-1.0 / (std::max(1.0, compAttackMs) * 0.001 * sampleRate));
    compReleaseCoeff = std::exp(-1.0 / (std::max(10.0, compReleaseMs) * 0.001 * sampleRate));

    // De-Esser Ballistics
    deEsserAttackCoeff = std::exp(-1.0 / (0.002 * sampleRate)); // 2 ms attack
    deEsserReleaseCoeff = std::exp(-1.0 / (0.050 * sampleRate)); // 50 ms release
}

void SourceChainProcessor::processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
    if (numSamples <= 0 || channels == nullptr) return;

    float* chL = channels[0];
    float* chR = (numChannels > 1) ? channels[1] : channels[0];

    // 1. Automatic Gain Staging (-18.0 dBFS RMS Normalization)
    gainStager.processBlock(channels, numChannels, numSamples);

    // 2. Subtractive Clean-Up EQ & High-Pass Filter
    if (hasHpf) {
        for (int i = 0; i < numSamples; ++i) {
            chL[i] = static_cast<float>(hpfL.processSample(static_cast<double>(chL[i])));
            if (numChannels > 1) {
                chR[i] = static_cast<float>(hpfR.processSample(static_cast<double>(chR[i])));
            }
        }
    }

    if (hasMudCut) {
        for (int i = 0; i < numSamples; ++i) {
            chL[i] = static_cast<float>(mudCutL.processSample(static_cast<double>(chL[i])));
            if (numChannels > 1) {
                chR[i] = static_cast<float>(mudCutR.processSample(static_cast<double>(chR[i])));
            }
        }
    }

    if (hasHarshNotch) {
        for (int i = 0; i < numSamples; ++i) {
            chL[i] = static_cast<float>(harshNotchL.processSample(static_cast<double>(chL[i])));
            if (numChannels > 1) {
                chR[i] = static_cast<float>(harshNotchR.processSample(static_cast<double>(chR[i])));
            }
        }
    }

    // 3. Automated Phase Rotation (Sub808 / Bass)
    if (hasPhaseRotator) {
        phaseRotator.processBlock(channels, numChannels, numSamples);
    }

    // 4. Cross-Adaptive Spectral Masking Ducking
    if (hasCrossDucking && sidechainPointers != nullptr && sidechainChannelCount > 0) {
        crossDucker.processBlock(channels, numChannels, sidechainPointers, sidechainChannelCount, numSamples);
    }

    // 5. Transient-Preserving Leveling Compression (VCA style)
    if (hasCompression) {
        double maxCompBlock = 0.0;
        for (int i = 0; i < numSamples; ++i) {
            const double sL = static_cast<double>(chL[i]);
            const double sR = static_cast<double>(chR[i]);
            const double level = std::max(std::abs(sL), std::abs(sR));
            const double levelDb = (level > 1.0e-5) ? 20.0 * std::log10(level) : -100.0;

            double targetGain = 1.0;
            if (levelDb > compThresholdDb) {
                const double overDb = (1.0 - 1.0 / compRatio) * (levelDb - compThresholdDb);
                targetGain = std::pow(10.0, -overDb / 20.0);
            }

            if (targetGain < compEnvelope) {
                compEnvelope = compAttackCoeff * compEnvelope + (1.0 - compAttackCoeff) * targetGain;
            } else {
                compEnvelope = compReleaseCoeff * compEnvelope + (1.0 - compReleaseCoeff) * targetGain;
            }

            const double grDb = (compEnvelope < 0.9999) ? -20.0 * std::log10(compEnvelope) : 0.0;
            if (grDb > maxCompBlock) maxCompBlock = grDb;

            chL[i] *= static_cast<float>(compEnvelope);
            if (numChannels > 1) {
                chR[i] *= static_cast<float>(compEnvelope);
            }
        }

        if (static_cast<float>(maxCompBlock) > maxCompGrSessionDb) {
            maxCompGrSessionDb = static_cast<float>(maxCompBlock);
        }
        currentCompGrDb = maxCompGrSessionDb;
    }

    // 6. Post-Comp Vocal De-Esser (5 - 9 kHz Dynamic Notch)
    if (hasDeEsser) {
        double maxDeEssBlock = 0.0;
        for (int i = 0; i < numSamples; ++i) {
            const double inL = static_cast<double>(chL[i]);
            const double inR = static_cast<double>(chR[i]);

            // Sidechain bandpass isolates sibilance
            const double sibL = deEsserBandpassL.processSample(inL);
            const double sibR = deEsserBandpassR.processSample(inR);
            const double sibLevel = std::max(std::abs(sibL), std::abs(sibR));

            if (sibLevel > deEsserEnvelope) {
                deEsserEnvelope = deEsserAttackCoeff * deEsserEnvelope + (1.0 - deEsserAttackCoeff) * sibLevel;
            } else {
                deEsserEnvelope = deEsserReleaseCoeff * deEsserEnvelope + (1.0 - deEsserReleaseCoeff) * sibLevel;
            }

            const double sibDb = (deEsserEnvelope > 1.0e-5) ? 20.0 * std::log10(deEsserEnvelope) : -100.0;
            double cutDb = 0.0;
            if (sibDb > -22.0) {
                cutDb = std::min(5.0, (sibDb - (-22.0)) * 0.6);
            }

            if (cutDb > maxDeEssBlock) maxDeEssBlock = cutDb;

            if (cutDb > 0.05) {
                const auto eqCoeffs = Biquad::makePeakEQ(sampleRate, 7200.0, 2.5, -cutDb);
                deEsserFilterL.setCoefficients(eqCoeffs);
                deEsserFilterR.setCoefficients(eqCoeffs);
                chL[i] = static_cast<float>(deEsserFilterL.processSample(inL));
                if (numChannels > 1) {
                    chR[i] = static_cast<float>(deEsserFilterR.processSample(inR));
                }
            }
        }

        if (static_cast<float>(maxDeEssBlock) > maxDeEsserSessionDb) {
            maxDeEsserSessionDb = static_cast<float>(maxDeEssBlock);
        }
        currentDeEsserGrDb = maxDeEsserSessionDb;
    }

    // 7. Harmonic Saturation (Sub808, Bass, Vocal)
    if (hasSaturation) {
        saturator.processBlock(channels, numChannels, numSamples);
    }

    // 8. Additive Sweetening EQ
    if (hasAdditiveEQ) {
        for (int i = 0; i < numSamples; ++i) {
            double sL = static_cast<double>(chL[i]);
            double sR = static_cast<double>(chR[i]);

            sL = boostLowL.processSample(sL);
            sL = boostHighL.processSample(sL);

            if (numChannels > 1) {
                sR = boostLowR.processSample(sR);
                sR = boostHighR.processSample(sR);
            }

            chL[i] = static_cast<float>(sL);
            if (numChannels > 1) {
                chR[i] = static_cast<float>(sR);
            }
        }
    }

    // 9. Spatial & Mono Collapse
    if (forceMono && numChannels > 1) {
        for (int i = 0; i < numSamples; ++i) {
            const float mono = (chL[i] + chR[i]) * 0.5f;
            chL[i] = mono;
            chR[i] = mono;
        }
    } else if (subMonoCollapse && numChannels > 1) {
        for (int i = 0; i < numSamples; ++i) {
            double lowL = 0.0, highL = 0.0;
            double lowR = 0.0, highR = 0.0;
            subXoverL.processSample(static_cast<double>(chL[i]), lowL, highL);
            subXoverR.processSample(static_cast<double>(chR[i]), lowR, highR);

            // Collapse low band to mono
            const double lowMono = (lowL + lowR) * 0.5;
            chL[i] = static_cast<float>(lowMono + highL);
            chR[i] = static_cast<float>(lowMono + highR);
        }
    }
    // 10. Output Makeup Gain to maintain -18.0 dBFS RMS target
    if (std::abs(outputMakeupGainLinear - 1.0f) > 1e-4f) {
        for (int i = 0; i < numSamples; ++i) {
            chL[i] *= outputMakeupGainLinear;
            if (numChannels > 1) {
                chR[i] *= outputMakeupGainLinear;
            }
        }
    }
}

} // namespace dsp::mix
