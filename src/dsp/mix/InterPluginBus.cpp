#include "InterPluginBus.h"

namespace dsp::mix {

InterPluginBus& InterPluginBus::getInstance() noexcept {
    static InterPluginBus instance;
    return instance;
}

int InterPluginBus::registerTrack(TrackClass trackClass, const char* name) noexcept {
    for (int i = 0; i < kMaxTracks; ++i) {
        bool expected = false;
        if (slots[static_cast<size_t>(i)].inUse.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            const int id = nextTrackIdCounter.fetch_add(1, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].trackId.store(id, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].trackClass.store(trackClass, std::memory_order_relaxed);

            if (name != nullptr && name[0] != '\0') {
                std::strncpy(slots[static_cast<size_t>(i)].trackName, name, 63);
                slots[static_cast<size_t>(i)].trackName[63] = '\0';
            } else {
                std::strncpy(slots[static_cast<size_t>(i)].trackName, trackClassToString(trackClass), 63);
            }

            slots[static_cast<size_t>(i)].inputRmsDbFS.store(-70.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].postGainRmsDbFS.store(-70.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].shortTermLUFS.store(-70.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].peakLevelDbFS.store(-70.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].gainStagingDb.store(0.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].dynamicEqCutDb.store(0.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].compressorGrDb.store(0.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].deEsserGrDb.store(0.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].phaseAngleDegrees.store(0.0f, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].isActive.store(false, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].isSidechainTriggered.store(false, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].sidechainWritePos.store(0, std::memory_order_relaxed);
            slots[static_cast<size_t>(i)].sidechainBuffer.fill(0.0f);

            return i;
        }
    }
    return -1; // No slots available
}

void InterPluginBus::unregisterTrack(int slotIndex) noexcept {
    if (slotIndex >= 0 && slotIndex < kMaxTracks) {
        slots[static_cast<size_t>(slotIndex)].inUse.store(false, std::memory_order_release);
        slots[static_cast<size_t>(slotIndex)].trackId.store(-1, std::memory_order_relaxed);
        slots[static_cast<size_t>(slotIndex)].trackClass.store(TrackClass::Unknown, std::memory_order_relaxed);
        slots[static_cast<size_t>(slotIndex)].isActive.store(false, std::memory_order_relaxed);
    }
}

void InterPluginBus::clearAll() noexcept {
    for (int i = 0; i < kMaxTracks; ++i) {
        unregisterTrack(i);
    }
    nextTrackIdCounter.store(1, std::memory_order_relaxed);
}

void InterPluginBus::publishTelemetry(int slotIndex, const TrackNodeTelemetry& telemetry) noexcept {
    if (slotIndex < 0 || slotIndex >= kMaxTracks) return;
    auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (!slot.inUse.load(std::memory_order_relaxed)) return;

    slot.trackClass.store(telemetry.trackClass, std::memory_order_relaxed);
    slot.inputRmsDbFS.store(telemetry.inputRmsDbFS, std::memory_order_relaxed);
    slot.postGainRmsDbFS.store(telemetry.postGainRmsDbFS, std::memory_order_relaxed);
    slot.shortTermLUFS.store(telemetry.shortTermLUFS, std::memory_order_relaxed);
    slot.peakLevelDbFS.store(telemetry.peakLevelDbFS, std::memory_order_relaxed);
    slot.gainStagingDb.store(telemetry.gainStagingDb, std::memory_order_relaxed);
    slot.dynamicEqCutDb.store(telemetry.dynamicEqCutDb, std::memory_order_relaxed);
    slot.compressorGrDb.store(telemetry.compressorGrDb, std::memory_order_relaxed);
    slot.deEsserGrDb.store(telemetry.deEsserGrDb, std::memory_order_relaxed);
    slot.phaseAngleDegrees.store(telemetry.phaseAngleDegrees, std::memory_order_relaxed);
    slot.isActive.store(telemetry.isActive, std::memory_order_relaxed);
    slot.isSidechainTriggered.store(telemetry.isSidechainTriggered, std::memory_order_relaxed);
}

void InterPluginBus::pushSidechainAudio(int slotIndex, const float* monoSamples, int numSamples) noexcept {
    if (slotIndex < 0 || slotIndex >= kMaxTracks || monoSamples == nullptr || numSamples <= 0) return;
    auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (!slot.inUse.load(std::memory_order_relaxed)) return;

    uint32_t writePos = slot.sidechainWritePos.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i) {
        slot.sidechainBuffer[writePos & (kSidechainRingSize - 1)] = monoSamples[i];
        writePos++;
    }
    slot.sidechainWritePos.store(writePos, std::memory_order_release);
}

int InterPluginBus::findSlotByClass(TrackClass trackClass) const noexcept {
    for (int i = 0; i < kMaxTracks; ++i) {
        if (slots[static_cast<size_t>(i)].inUse.load(std::memory_order_acquire) &&
            slots[static_cast<size_t>(i)].trackClass.load(std::memory_order_relaxed) == trackClass) {
            return i;
        }
    }
    return -1;
}

bool InterPluginBus::getTelemetry(int slotIndex, TrackNodeTelemetry& dest) const noexcept {
    if (slotIndex < 0 || slotIndex >= kMaxTracks) return false;
    const auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (!slot.inUse.load(std::memory_order_acquire)) return false;

    dest.trackId = slot.trackId.load(std::memory_order_relaxed);
    dest.trackClass = slot.trackClass.load(std::memory_order_relaxed);
    std::strncpy(dest.trackName, slot.trackName, 63);
    dest.trackName[63] = '\0';
    dest.inputRmsDbFS = slot.inputRmsDbFS.load(std::memory_order_relaxed);
    dest.postGainRmsDbFS = slot.postGainRmsDbFS.load(std::memory_order_relaxed);
    dest.shortTermLUFS = slot.shortTermLUFS.load(std::memory_order_relaxed);
    dest.peakLevelDbFS = slot.peakLevelDbFS.load(std::memory_order_relaxed);
    dest.gainStagingDb = slot.gainStagingDb.load(std::memory_order_relaxed);
    dest.dynamicEqCutDb = slot.dynamicEqCutDb.load(std::memory_order_relaxed);
    dest.compressorGrDb = slot.compressorGrDb.load(std::memory_order_relaxed);
    dest.deEsserGrDb = slot.deEsserGrDb.load(std::memory_order_relaxed);
    dest.phaseAngleDegrees = slot.phaseAngleDegrees.load(std::memory_order_relaxed);
    dest.isActive = slot.isActive.load(std::memory_order_relaxed);
    dest.isSidechainTriggered = slot.isSidechainTriggered.load(std::memory_order_relaxed);

    return true;
}

int InterPluginBus::readSidechainAudio(int slotIndex, float* dest, int numSamples) const noexcept {
    if (slotIndex < 0 || slotIndex >= kMaxTracks || dest == nullptr || numSamples <= 0) return 0;
    const auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (!slot.inUse.load(std::memory_order_acquire)) return 0;

    const uint32_t writePos = slot.sidechainWritePos.load(std::memory_order_acquire);
    const int available = std::min(numSamples, kSidechainRingSize);

    // Read most recent samples ending at writePos
    uint32_t readPos = writePos - static_cast<uint32_t>(available);
    for (int i = 0; i < available; ++i) {
        dest[i] = slot.sidechainBuffer[readPos & (kSidechainRingSize - 1)];
        readPos++;
    }
    return available;
}

int InterPluginBus::getActiveTrackCount() const noexcept {
    int count = 0;
    for (int i = 0; i < kMaxTracks; ++i) {
        if (slots[static_cast<size_t>(i)].inUse.load(std::memory_order_acquire)) {
            count++;
        }
    }
    return count;
}

float InterPluginBus::getVocalRelativeLoudness(float masterShortTermLUFS) const noexcept {
    const int vocalSlot = findSlotByClass(TrackClass::LeadVocal);
    if (vocalSlot < 0) return 0.0f;

    const float vocalLUFS = slots[static_cast<size_t>(vocalSlot)].shortTermLUFS.load(std::memory_order_relaxed);
    if (vocalLUFS <= -60.0f || masterShortTermLUFS <= -60.0f) {
        return 0.0f;
    }
    return vocalLUFS - masterShortTermLUFS; // Nominal: -3.0 LU
}

} // namespace dsp::mix
