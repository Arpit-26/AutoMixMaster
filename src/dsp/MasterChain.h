#pragma once
#include "InputStage.h"
#include "CorrectiveEQ.h"
#include "MixBusGlueCompressor.h"
#include "LowBandDynamicProcessor.h"
#include "StereoWidthEnforcer.h"
#include "HarmonicSaturator.h"
#include "TonalBalanceEQ.h"
#include "DynamicResonanceSuppressor.h"
#include "OversampledSoftClipper.h"
#include "TruePeakLimiter.h"
#include "LoudnessMeter.h"
#include "TruePeakMeter.h"
#include "SpectralAnalyzer.h"
#include "SpatialAnalyzer.h"
#include "TelemetrySnapshot.h"

namespace dsp {

/**
 * 10-Stage Master Chain and Closed-Loop Loudness Optimizer.
 * Coordinates stages 1 through 10 in a strictly real-time audio thread safe pipeline
 * with zero heap allocations, pre-allocated delay lines, and integrated telemetry.
 */
class MasterChain {
public:
    enum class MasteringPreset {
        StreamingSafe, // -14.0 LUFS, -1.0 dBTP ceiling, PSR >= 10.0
        Competitive,   // -10.0 LUFS, -2.0 dBTP ceiling, PSR >= 8.0
        Dynamic        // -16.0 LUFS, -1.0 dBTP ceiling, PSR >= 12.0
    };

    MasterChain() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setPreset(MasteringPreset preset) noexcept;
    void setTargetLUFS(double targetLUFS) noexcept;
    void setCeilingdBTP(double ceilingDbTP) noexcept;

    void setPostGainMakeupLinear(float gain) noexcept { postGainMakeupLinear = gain; }
    [[nodiscard]] float getPostGainMakeupLinear() const noexcept { return postGainMakeupLinear; }

    // Real-time audio thread processing: zero heap allocations
    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept;

    // Closed-loop optimization pass: adapts thresholds and gains based on analyzed telemetry
    void optimizeClosedLoop(const TelemetrySnapshot& inputTelemetry,
                            const LoudnessResult& inputLoudness) noexcept;

    [[nodiscard]] int getLatencySamples() const noexcept {
        return stage10Limiter.getLatencySamples();
    }

    // Telemetry getters
    [[nodiscard]] float getLimiterGainReduction_dB() const noexcept { return stage10Limiter.getGainReduction_dB(); }
    [[nodiscard]] float getGlueGainReduction_dB() const noexcept { return stage3Glue.getGainReduction_dB(); }
    [[nodiscard]] float getSoftClipperShave_dB() const noexcept { return stage9Clipper.getPeakShaved_dB(); }
    [[nodiscard]] float getLowBandGainReduction_dB() const noexcept { return stage4LowBand.getLowBandGainReduction_dB(); }
    [[nodiscard]] float getResonanceMaxCut_dB() const noexcept { return stage8Resonance.getMaxCutApplied_dB(); }

    // Direct access to individual stages for expert automation
    InputStage& getInputStage() noexcept { return stage1Input; }
    CorrectiveEQ& getCorrectiveEQ() noexcept { return stage2CorrectiveEQ; }
    MixBusGlueCompressor& getMixBusGlue() noexcept { return stage3Glue; }
    LowBandDynamicProcessor& getLowBandDynamics() noexcept { return stage4LowBand; }
    StereoWidthEnforcer& getStereoWidth() noexcept { return stage5Width; }
    HarmonicSaturator& getHarmonicSaturator() noexcept { return stage6Saturation; }
    TonalBalanceEQ& getTonalBalanceEQ() noexcept { return stage7TonalEQ; }
    DynamicResonanceSuppressor& getResonanceSuppressor() noexcept { return stage8Resonance; }
    OversampledSoftClipper& getSoftClipper() noexcept { return stage9Clipper; }
    TruePeakLimiter& getTruePeakLimiter() noexcept { return stage10Limiter; }

private:
    double sampleRate = 48000.0;
    MasteringPreset currentPreset = MasteringPreset::StreamingSafe;

    double targetLoudnessLUFS = -14.0;
    double targetCeilingDbTP = -1.0;
    float postGainMakeupLinear = 1.0f;

    // 10 Mastering Stages
    InputStage                  stage1Input;
    CorrectiveEQ                stage2CorrectiveEQ;
    MixBusGlueCompressor        stage3Glue;
    LowBandDynamicProcessor     stage4LowBand;
    StereoWidthEnforcer         stage5Width;
    HarmonicSaturator           stage6Saturation;
    TonalBalanceEQ              stage7TonalEQ;
    DynamicResonanceSuppressor  stage8Resonance;
    OversampledSoftClipper      stage9Clipper;
    TruePeakLimiter             stage10Limiter;
};

} // namespace dsp
