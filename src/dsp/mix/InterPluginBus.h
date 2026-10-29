#pragma once
#include "TrackNodeTypes.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace dsp::mix {

/**
 * Lock-free inter-plugin shared memory bus.
 * Coordinates cross-instance communication across FL Studio mixer inserts with
 * zero heap allocations and zero blocking mutexes on the audio thread.
 */
class InterPluginBus {
public:
    static constexpr int kMaxTracks = 64;
    static constexpr int kSidechainRingSize = 4096; // Power of 2 for fast wrapping

    struct TrackSlot {
        std::atomic<bool> inUse { false };
        std::atomic<int> trackId { -1 };
        std::atomic<TrackClass> trackClass { TrackClass::Unknown };
        char trackName[64] = { 0 };

        std::atomic<float> inputRmsDbFS { -70.0f };
        std::atomic<float> postGainRmsDbFS { -70.0f };
        std::atomic<float> shortTermLUFS { -70.0f };
        std::atomic<float> peakLevelDbFS { -70.0f };

        std::atomic<float> gainStagingDb { 0.0f };
        std::atomic<float> dynamicEqCutDb { 0.0f };
        std::atomic<float> compressorGrDb { 0.0f };
        std::atomic<float> deEsserGrDb { 0.0f };
        std::atomic<float> phaseAngleDegrees { 0.0f };

        std::atomic<bool> isActive { false };
        std::atomic<bool> isSidechainTriggered { false };

        // Sidechain lock-free audio ring buffer for cross-track dynamic EQ & phase alignment
        std::array<float, kSidechainRingSize> sidechainBuffer {};
        std::atomic<uint32_t> sidechainWritePos { 0 };
    };

    static InterPluginBus& getInstance() noexcept;

    // Registration (Called on prepareToPlay / releaseResources)
    int registerTrack(TrackClass trackClass, const char* name = nullptr) noexcept;
    void unregisterTrack(int slotIndex) noexcept;
    void clearAll() noexcept;

    // Track Node Publisher API (Real-time audio thread safe: zero heap allocations)
    void publishTelemetry(int slotIndex, const TrackNodeTelemetry& telemetry) noexcept;
    void pushSidechainAudio(int slotIndex, const float* monoSamples, int numSamples) noexcept;

    // Cross-Track Consumer API (Query sidechain audio and telemetry across tracks)
    int findSlotByClass(TrackClass trackClass) const noexcept;
    bool getTelemetry(int slotIndex, TrackNodeTelemetry& dest) const noexcept;
    int readSidechainAudio(int slotIndex, float* dest, int numSamples) const noexcept;

    // Master Bus Query API
    int getActiveTrackCount() const noexcept;
    float getVocalRelativeLoudness(float masterShortTermLUFS) const noexcept;

private:
    InterPluginBus() noexcept = default;
    ~InterPluginBus() noexcept = default;

    std::array<TrackSlot, kMaxTracks> slots;
    std::atomic<int> nextTrackIdCounter { 1 };
};

} // namespace dsp::mix
