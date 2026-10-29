#pragma once
#include <array>

namespace dsp {

struct TelemetrySnapshot {
    // ITU-R BS.1770-4 Loudness Metrics
    float momentaryLUFS       = -70.0f;
    float shortTermLUFS       = -70.0f;
    float maxShortTermLUFS    = -70.0f;
    float integratedLUFS      = -70.0f;
    float loudnessRangeLU     = 0.0f;    // EBU Tech 3342 LRA (10th to 95th percentile)
    float truePeakdBTP        = -100.0f; // 4x Polyphase FIR True Peak
    float plrLU               = 0.0f;    // Peak-to-Loudness Ratio (True Peak - Integrated)
    float minPsrLU            = 100.0f;  // Peak-to-Short-Term Ratio (in loudest 30s)

    // Spectral & Dynamics Telemetry
    float lowBandCrest_dB     = 0.0f;    // Crest factor below 250 Hz
    float spectralTiltSlope   = -4.5f;   // Commercial roll-off slope (dB/octave)
    std::array<float, 4> macroBandEnergies_dB = { 0.0f, 0.0f, 0.0f, 0.0f }; // [<250Hz, 250-2k, 2k-8k, >8k]

    // Spatial & Phase Telemetry
    std::array<float, 4> bandCorrelation = { 1.0f, 1.0f, 1.0f, 1.0f }; // [<120Hz, 120-1k, 1k-5k, >5k]
    float globalCorrelation   = 1.0f;
    float subMonoSumLoss_dB   = 0.0f;    // Stereo vs (L+R)/2 loudness loss below 120 Hz
    bool  monoIncompatibility = false;   // True if subMonoSumLoss > 1.0 dB

    // FL Studio Wrapper Diagnostics
    bool  sidechainActive     = false;
};

} // namespace dsp
