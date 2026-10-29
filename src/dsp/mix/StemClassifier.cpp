#include "StemClassifier.h"
#include <algorithm>
#include <cctype>

namespace dsp::mix {

static std::string toLower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

TrackClass StemClassifier::classifyFromTrackName(std::string_view rawName) noexcept {
    if (rawName.empty()) {
        return TrackClass::Unknown;
    }

    const std::string name = toLower(rawName);

    // 1. Backing Vocals (check before lead to catch "bg vox", "backing vocal")
    if (name.find("back") != std::string::npos || name.find("bgv") != std::string::npos ||
        name.find("bkv") != std::string::npos || name.find("harmony") != std::string::npos ||
        name.find("adlib") != std::string::npos || name.find("double") != std::string::npos ||
        name.find("bg vox") != std::string::npos) {
        return TrackClass::BackingVocal;
    }

    // 2. Lead Vocal
    if (name.find("lead_vocal") != std::string::npos || name.find("lead vocal") != std::string::npos ||
        name.find("vocal") != std::string::npos || name.find("vox") != std::string::npos ||
        name.find("acapella") != std::string::npos || name.find("lead_vox") != std::string::npos ||
        name.find("main_vox") != std::string::npos) {
        return TrackClass::LeadVocal;
    }

    // 3. 808 / Sub-Bass (check before general bass and kick)
    if (name.find("808") != std::string::npos || name.find("subbass") != std::string::npos ||
        name.find("sub_bass") != std::string::npos || name.find("sub-bass") != std::string::npos ||
        name.find("sub bass") != std::string::npos || name.find("sub") != std::string::npos) {
        return TrackClass::Sub808;
    }

    // 4. Kick Drum
    if (name.find("kick") != std::string::npos || name.find("bd") != std::string::npos ||
        name.find("bassdrum") != std::string::npos || name.find("kik") != std::string::npos) {
        return TrackClass::Kick;
    }

    // 5. Snare Drum
    if (name.find("snare") != std::string::npos || name.find("sd") != std::string::npos ||
        name.find("rim") != std::string::npos || name.find("clap") != std::string::npos ||
        name.find("snap") != std::string::npos) {
        return TrackClass::Snare;
    }

    // 6. Bass Guitar / General Bass
    if (name.find("bass") != std::string::npos || name.find("reese") != std::string::npos ||
        name.find("slap") != std::string::npos || name.find("fretless") != std::string::npos) {
        return TrackClass::Bass;
    }

    // 7. Hi-Hats / Cymbals
    if (name.find("hat") != std::string::npos || name.find("hh") != std::string::npos ||
        name.find("cymbal") != std::string::npos || name.find("ride") != std::string::npos ||
        name.find("crash") != std::string::npos || name.find("shaker") != std::string::npos ||
        name.find("tamb") != std::string::npos || name.find("perc") != std::string::npos) {
        return TrackClass::HiHats;
    }

    // 8. Pads / Keys / Guitars
    if (name.find("pad") != std::string::npos || name.find("key") != std::string::npos ||
        name.find("piano") != std::string::npos || name.find("guitar") != std::string::npos ||
        name.find("gtr") != std::string::npos || name.find("rhodes") != std::string::npos ||
        name.find("ep") != std::string::npos || name.find("organ") != std::string::npos ||
        name.find("string") != std::string::npos || name.find("synth_pad") != std::string::npos) {
        return TrackClass::PadsKeysGuitars;
    }

    // 9. Lead Synth
    if (name.find("synth") != std::string::npos || name.find("lead") != std::string::npos ||
        name.find("arp") != std::string::npos || name.find("pluck") != std::string::npos ||
        name.find("supersaw") != std::string::npos) {
        return TrackClass::LeadSynth;
    }

    // 10. Drum Bus
    if (name.find("drum bus") != std::string::npos || name.find("drumbus") != std::string::npos ||
        name.find("drums") != std::string::npos) {
        return TrackClass::DrumBus;
    }

    // 11. Mix Bus
    if (name.find("mix bus") != std::string::npos || name.find("mixbus") != std::string::npos ||
        name.find("master") != std::string::npos || name.find("premaster") != std::string::npos) {
        return TrackClass::MixBus;
    }

    return TrackClass::Unknown;
}

TrackClass StemClassifier::classifyFromAcousticFeatures(float lowEnergyRatio,
                                                        float spectralCentroidHz,
                                                        float crestFactorDb,
                                                        float /*rmsDbFS*/) noexcept {
    // 1. Dominant Low Frequency Material (<250 Hz energy ratio > 55%)
    if (lowEnergyRatio > 0.55f) {
        if (crestFactorDb > 11.0f) {
            // Highly impulsive low-end -> Kick Drum or 808 bursts
            return (spectralCentroidHz < 150.0f) ? TrackClass::Sub808 : TrackClass::Kick;
        }
        return TrackClass::Bass;
    }

    // 2. High Frequency Material (Centroid > 4500 Hz, very low bass)
    if (spectralCentroidHz > 4500.0f && lowEnergyRatio < 0.15f) {
        return TrackClass::HiHats;
    }

    // 3. Midrange Material (Centroid 1000 - 3500 Hz)
    if (spectralCentroidHz >= 800.0f && spectralCentroidHz <= 3500.0f) {
        if (crestFactorDb > 11.0f) {
            return TrackClass::Snare;
        }
        if (crestFactorDb >= 7.0f && crestFactorDb <= 11.0f) {
            return TrackClass::LeadVocal;
        }
        return TrackClass::PadsKeysGuitars;
    }

    // 4. Default fallback
    return TrackClass::PadsKeysGuitars;
}

TrackClass StemClassifier::classify(std::string_view name,
                                    float lowEnergyRatio,
                                    float spectralCentroidHz,
                                    float crestFactorDb,
                                    float rmsDbFS) noexcept {
    const TrackClass fromName = classifyFromTrackName(name);
    if (fromName != TrackClass::Unknown) {
        return fromName;
    }
    return classifyFromAcousticFeatures(lowEnergyRatio, spectralCentroidHz, crestFactorDb, rmsDbFS);
}

} // namespace dsp::mix
