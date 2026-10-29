#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace dsp::mix {

/**
 * 11 Semantic Source Classes defined in docs/architecture_spec.md
 */
enum class TrackClass : uint8_t {
    LeadVocal = 0,
    BackingVocal,
    Kick,
    Snare,
    Sub808,
    Bass,
    HiHats,
    PadsKeysGuitars,
    LeadSynth,
    DrumBus,
    MixBus,
    Unknown
};

/**
 * Plugin Operating Mode: MasterBus mastering console vs. individual TrackNode mixer tap.
 */
enum class PluginOperatingMode : uint8_t {
    MasterBus = 0,
    TrackNode
};

inline const char* trackClassToString(TrackClass c) noexcept {
    switch (c) {
        case TrackClass::LeadVocal:       return "Lead Vocal";
        case TrackClass::BackingVocal:    return "Backing Vocal";
        case TrackClass::Kick:            return "Kick Drum";
        case TrackClass::Snare:           return "Snare Drum";
        case TrackClass::Sub808:          return "808 / Sub-Bass";
        case TrackClass::Bass:            return "Bass Guitar";
        case TrackClass::HiHats:          return "Hi-Hats / Cymbals";
        case TrackClass::PadsKeysGuitars: return "Pads / Keys / Guitars";
        case TrackClass::LeadSynth:       return "Lead Synth";
        case TrackClass::DrumBus:         return "Drum Bus";
        case TrackClass::MixBus:          return "Mix Bus";
        case TrackClass::Unknown:
        default:                          return "Unknown";
    }
}

/**
 * Real-time atomic-safe snapshot of track telemetry published to InterPluginBus.
 */
struct TrackNodeTelemetry {
    int trackId = -1;
    TrackClass trackClass = TrackClass::Unknown;
    char trackName[64] = { 0 };

    float inputRmsDbFS = -70.0f;
    float postGainRmsDbFS = -70.0f;
    float shortTermLUFS = -70.0f;
    float peakLevelDbFS = -70.0f;

    float gainStagingDb = 0.0f;
    float dynamicEqCutDb = 0.0f;
    float compressorGrDb = 0.0f;
    float deEsserGrDb = 0.0f;
    float phaseAngleDegrees = 0.0f;

    bool isActive = false;
    bool isSidechainTriggered = false;
};

} // namespace dsp::mix
