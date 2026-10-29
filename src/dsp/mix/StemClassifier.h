#pragma once
#include "TrackNodeTypes.h"
#include <string>
#include <string_view>

namespace dsp::mix {

/**
 * Hybrid Stem Classifier combining heuristic track name string parsing with
 * real-time acoustic feature extraction fallback (spectral centroid, crest factor, low energy ratio).
 */
class StemClassifier {
public:
    StemClassifier() noexcept = default;

    /**
     * Parses track name string (e.g. from DAW channel strip or WAV stem filename).
     */
    static TrackClass classifyFromTrackName(std::string_view name) noexcept;

    /**
     * Fallback classifier based on measured acoustic features.
     */
    static TrackClass classifyFromAcousticFeatures(float lowEnergyRatio,
                                                   float spectralCentroidHz,
                                                   float crestFactorDb,
                                                   float rmsDbFS) noexcept;

    /**
     * Hybrid classification: uses track name if high-confidence, otherwise falls back to acoustic features.
     */
    static TrackClass classify(std::string_view name,
                               float lowEnergyRatio,
                               float spectralCentroidHz,
                               float crestFactorDb,
                               float rmsDbFS) noexcept;
};

} // namespace dsp::mix
