#pragma once
#include "TrackNodeTypes.h"
#include "GainStager.h"
#include "PhaseRotator.h"
#include "CrossAdaptiveDucker.h"
#include "../Biquad.h"
#include "../HarmonicSaturator.h"
#include <memory>

namespace dsp::mix {

/**
 * The 11 Source-Specific DSP Chains defined in Section 2.4 of docs/architecture_spec.md.
 * Provides transparent, high-fidelity mixing processing tailored per semantic stem:
 *   - Automatic -18.0 dBFS RMS gain staging
 *   - Subtractive cleanup EQ & HPF
 *   - Transient-preserving leveling compression (5-15 ms attack)
 *   - Post-comp vocal de-essing (5-9 kHz)
 *   - Cross-adaptive dynamic EQ unmasking (Kick->Bass 60-150 Hz, Vocal->Music 1-4 kHz)
 *   - Automated phase rotation (0°-360°)
 *   - Harmonic saturation for small speaker translation
 *   - Additive sweetening EQ & sub-mono collapse
 */
class SourceChainProcessor {
public:
    SourceChainProcessor() noexcept = default;

    void prepare(double sampleRate, TrackClass trackClass) noexcept;
    void reset() noexcept;

    void setTrackClass(TrackClass newClass) noexcept;
    [[nodiscard]] TrackClass getTrackClass() const noexcept { return currentClass; }

    // Sidechain and cross-track routing
    void setSidechainKey(const float* const* scChannels, int numScChannels) noexcept;
    void optimizeLowEndPhase(const float* bassAudio, const float* kickAudio, int numSamples) noexcept;
    void optimizeLowEndPhase(const float* kickAudio, int numSamples) noexcept;

    // Real-time audio thread processing: zero heap allocations
    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept;

    // Direct component access for testing & telemetry
    [[nodiscard]] PhaseRotator& getPhaseRotator() noexcept { return phaseRotator; }
    [[nodiscard]] const PhaseRotator& getPhaseRotator() const noexcept { return phaseRotator; }
    [[nodiscard]] GainStager& getGainStager() noexcept { return gainStager; }
    [[nodiscard]] const GainStager& getGainStager() const noexcept { return gainStager; }
    [[nodiscard]] CrossAdaptiveDucker& getCrossDucker() noexcept { return crossDucker; }
    [[nodiscard]] const CrossAdaptiveDucker& getCrossDucker() const noexcept { return crossDucker; }

    // Telemetry getters
    [[nodiscard]] float getInputRmsDbFS() const noexcept { return gainStager.getInputRmsDbFS(); }
    [[nodiscard]] float getPostGainRmsDbFS() const noexcept { return gainStager.getPostGainRmsDbFS(); }
    [[nodiscard]] float getGainStagingDb() const noexcept { return gainStager.getGainStagingDb(); }
    [[nodiscard]] float getCompressorGrDb() const noexcept { return currentCompGrDb; }
    [[nodiscard]] float getDeEsserGrDb() const noexcept { return currentDeEsserGrDb; }
    [[nodiscard]] float getDynamicEqCutDb() const noexcept { return crossDucker.getMaxCutDb(); }
    [[nodiscard]] float getMaxDynamicEqCutDb() const noexcept { return crossDucker.getMaxCutDb(); }
    [[nodiscard]] float getPhaseAngleDegrees() const noexcept { return phaseRotator.getAngleDegrees(); }

private:
    void setupChainForClass() noexcept;

    double sampleRate = 48000.0;
    TrackClass currentClass = TrackClass::Unknown;

    GainStager          gainStager;
    PhaseRotator        phaseRotator;
    CrossAdaptiveDucker crossDucker;
    HarmonicSaturator   saturator;

    // Subtractive Clean-up EQ
    Biquad hpfL, hpfR;
    Biquad mudCutL, mudCutR;
    Biquad harshNotchL, harshNotchR;

    // Additive EQ
    Biquad boostLowL, boostLowR;
    Biquad boostHighL, boostHighR;

    // Compression ballistics
    double compThresholdDb = -18.0;
    double compRatio = 3.0;
    double compAttackCoeff = 0.0;
    double compReleaseCoeff = 0.0;
    double compEnvelope = 1.0;
    float currentCompGrDb = 0.0f;
    float maxCompGrSessionDb = 0.0f;

    // De-Esser (Vocal)
    Biquad deEsserBandpassL, deEsserBandpassR;
    Biquad deEsserFilterL, deEsserFilterR;
    double deEsserAttackCoeff = 0.0;
    double deEsserReleaseCoeff = 0.0;
    double deEsserEnvelope = 0.0;
    float currentDeEsserGrDb = 0.0f;
    float maxDeEsserSessionDb = 0.0f;

    // Sidechain key pointers
    const float* const* sidechainPointers = nullptr;
    int sidechainChannelCount = 0;

    // Flags
    bool hasHpf = true;
    bool hasMudCut = false;
    bool hasHarshNotch = false;
    bool hasCompression = true;
    bool hasDeEsser = false;
    bool hasCrossDucking = false;
    bool hasPhaseRotator = false;
    bool hasSaturation = false;
    bool hasAdditiveEQ = false;
    bool forceMono = false;
    bool subMonoCollapse = false;

    float outputMakeupGainLinear = 1.0f;

    LinkwitzRiley4thOrder subXoverL, subXoverR;
};

} // namespace dsp::mix
