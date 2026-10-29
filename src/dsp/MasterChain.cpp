#include "MasterChain.h"
#include <algorithm>
#include <cmath>

namespace dsp {

void MasterChain::prepare(double sRate, int /*maxBlockSize*/) {
    sampleRate = sRate;

    stage1Input.prepare(sampleRate);
    stage2CorrectiveEQ.prepare(sampleRate);
    stage3Glue.prepare(sampleRate);
    stage4LowBand.prepare(sampleRate);
    stage5Width.prepare(sampleRate);
    stage6Saturation.prepare(sampleRate);
    stage7TonalEQ.prepare(sampleRate);
    stage8Resonance.prepare(sampleRate);
    stage9Clipper.prepare(sampleRate);
    stage10Limiter.prepare(sampleRate, 2, 5.0); // 5 ms lookahead

    postGainMakeupLinear = 1.0f;
    setPreset(MasteringPreset::StreamingSafe);
    reset();
}

void MasterChain::reset() noexcept {
    stage1Input.reset();
    stage2CorrectiveEQ.reset();
    stage3Glue.reset();
    stage4LowBand.reset();
    stage5Width.reset();
    stage6Saturation.reset();
    stage7TonalEQ.reset();
    stage8Resonance.reset();
    stage9Clipper.reset();
    stage10Limiter.reset();
}

void MasterChain::setPreset(MasteringPreset preset) noexcept {
    currentPreset = preset;
    switch (preset) {
        case MasteringPreset::StreamingSafe:
            targetLoudnessLUFS = -14.0;
            targetCeilingDbTP = -1.0;
            stage10Limiter.setCeilingdBTP(-1.0);
            stage9Clipper.setThresholdDb(-1.5);
            stage3Glue.setParameters(-14.0, 1.6, 25.0, 150.0);
            stage7TonalEQ.setProfile(TonalBalanceEQ::TargetProfile::Modern);
            break;

        case MasteringPreset::Competitive:
            targetLoudnessLUFS = -10.0;
            targetCeilingDbTP = -2.0;
            stage10Limiter.setCeilingdBTP(-2.0);
            // More aggressive soft-clipping and bus glue to protect PSR >= 8.0 without limiter pumping
            stage9Clipper.setThresholdDb(-3.0);
            stage3Glue.setParameters(-17.0, 2.0, 20.0, 120.0);
            stage7TonalEQ.setProfile(TonalBalanceEQ::TargetProfile::Modern);
            break;

        case MasteringPreset::Dynamic:
            targetLoudnessLUFS = -16.0;
            targetCeilingDbTP = -1.0;
            stage10Limiter.setCeilingdBTP(-1.0);
            stage9Clipper.setThresholdDb(-1.0);
            stage3Glue.setParameters(-12.0, 1.4, 30.0, 200.0);
            stage7TonalEQ.setProfile(TonalBalanceEQ::TargetProfile::Orchestral);
            break;
    }
}

void MasterChain::setTargetLUFS(double targetLUFS) noexcept {
    targetLoudnessLUFS = targetLUFS;
}

void MasterChain::setCeilingdBTP(double ceilingDbTP) noexcept {
    targetCeilingDbTP = ceilingDbTP;
    stage10Limiter.setCeilingdBTP(ceilingDbTP);
}

void MasterChain::processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
    // Stage 1: Input Trim, Pre-Limit Guard & 20 Hz Subsonic HPF
    stage1Input.processBlock(channels, numChannels, numSamples);

    // Stage 2: Corrective Surgical EQ
    stage2CorrectiveEQ.processBlock(channels, numChannels, numSamples);

    // Stage 3: Mix-Bus Glue Compressor
    stage3Glue.processBlock(channels, numChannels, numSamples);

    // Stage 4: Low-Band Crest Multiband Compressor & Dynamic EQ
    stage4LowBand.processBlock(channels, numChannels, numSamples);

    // Stage 5: Mid/Side Crossover & Stereo Width Enforcer
    stage5Width.processBlock(channels, numChannels, numSamples);

    // Stage 6: Band-Limited Harmonic Saturation
    stage6Saturation.processBlock(channels, numChannels, numSamples);

    // Stage 7: 4-Band Tonal-Balance Target EQ
    stage7TonalEQ.processBlock(channels, numChannels, numSamples);

    // Stage 8: Post-Compression Dynamic Resonance Suppressor
    stage8Resonance.processBlock(channels, numChannels, numSamples);

    // Intermediate Loudness Makeup Gain
    if (std::abs(postGainMakeupLinear - 1.0f) > 1e-4f) {
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = channels[ch];
            for (int i = 0; i < numSamples; ++i) {
                data[i] *= postGainMakeupLinear;
            }
        }
    }

    // Stage 9: Oversampled Soft Clipper (shaving 1-2 dB off fast percussive peaks)
    stage9Clipper.processBlock(channels, numChannels, numSamples);

    // Stage 10: 4x Polyphase FIR Lookahead True-Peak Limiter
    stage10Limiter.processBlock(channels, numChannels, numSamples);
}

void MasterChain::optimizeClosedLoop(const TelemetrySnapshot& inputTelemetry,
                                     const LoudnessResult& inputLoudness) noexcept {
    // 1. Evaluate Input Pre-Limit Guard & Input Trim
    stage1Input.configureFromInputTelemetry(inputTelemetry.plrLU,
                                            inputLoudness.integratedLUFS,
                                            std::pow(10.0f, inputTelemetry.truePeakdBTP / 20.0f));

    // 2. Compute Loudness Gap & Post-Gain Makeup
    const double currentLufs = (inputLoudness.integratedLUFS > -70.0) ? inputLoudness.integratedLUFS : -24.0;
    const double deltaLufs = targetLoudnessLUFS - currentLufs;
    const double trimDb = (stage1Input.getTrimGainLinear() > 1e-4f) ? 20.0 * std::log10(stage1Input.getTrimGainLinear()) : 0.0;
    const double netGainNeeded = deltaLufs - trimDb;

    postGainMakeupLinear = static_cast<float>(std::pow(10.0, netGainNeeded / 20.0));

    // 3. Stereo Width & Sub-Band Mono-Sum Loss Guard
    // If sub-band (<120 Hz) mono loss exceeds 1.0 dB or sub correlation < 0.3, enforce 100% mono sub-bass
    if (inputTelemetry.subMonoSumLoss_dB > 1.0f || inputTelemetry.bandCorrelation[0] < 0.3f) {
        stage5Width.setSubMonoCollapse(true);
    }

    // 4. Low-Band Crest Factor Control (<250 Hz)
    // If low crest factor > 12 dB (loose dynamic bass spikes), tighten low-band compression
    if (inputTelemetry.lowBandCrest_dB > 10.0f) {
        const double lowThresh = -18.0 - (inputTelemetry.lowBandCrest_dB - 10.0f) * 0.5;
        stage4LowBand.setLowBandThreshold(std::max(-24.0, lowThresh));
    }

    // 5. Tonal Balance & Mud / Harshness Correction
    // Check spectral tilt slope: if too dark or too bright vs -4.5 dB/oct
    const double tiltDiff = inputTelemetry.spectralTiltSlope - (-4.5);
    double lowShelfAdj = 0.0;
    double highShelfAdj = 0.0;
    double mudCutDb = 0.0;

    if (tiltDiff > 1.5) {
        // Excess treble / deficient bass
        lowShelfAdj = std::clamp(tiltDiff * 0.8, 0.0, 2.5);
        highShelfAdj = std::clamp(-tiltDiff * 0.8, -2.5, 0.0);
    } else if (tiltDiff < -1.5) {
        // Excess bass / dark treble
        lowShelfAdj = std::clamp(tiltDiff * 0.8, -2.5, 0.0);
        highShelfAdj = std::clamp(-tiltDiff * 0.8, 0.0, 2.5);
    }

    // If there's an excessive mud buildup around 250 Hz (detected in spectral analysis)
    if (inputTelemetry.macroBandEnergies_dB[0] > inputTelemetry.macroBandEnergies_dB[1] + 6.0f) {
        mudCutDb = -2.0; // Surgical -2 dB cut at 250 Hz (capped at 3 dB)
    }

    stage7TonalEQ.setBandGains(lowShelfAdj, mudCutDb, 0.0, highShelfAdj);

    // 6. Limiter Offloading & Soft-Clipper Optimization for High Target Loudness
    if (currentPreset == MasteringPreset::Competitive || netGainNeeded > 6.0) {
        // Offload 1.5 to 2.5 dB of gain reduction to soft-clipper and glue compressor
        stage9Clipper.setThresholdDb(-3.0);
        stage3Glue.setParameters(-16.0, 2.0, 20.0, 100.0);
    } else {
        stage9Clipper.setThresholdDb(-1.5);
    }

    // 7. Enforce True-Peak Ceiling
    stage10Limiter.setCeilingdBTP(targetCeilingDbTP);
}

} // namespace dsp
