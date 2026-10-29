#include "LoudnessMeter.h"
#include <numeric>

namespace dsp {

void LoudnessMeter::prepare(double sRate, int /*maxBlockSize*/, double maxDurationSeconds) {
    sampleRate = sRate;
    hopSamples100ms = std::max(1, static_cast<int>(std::round(sampleRate * 0.100)));
    sampleCounter = 0;
    accumEnergyL = accumEnergyR = 0.0;

    for (auto& kf : kFilters) {
        kf.prepare(sampleRate);
    }

    momentaryRingBuffer.fill(0.0);
    momentaryRingIdx = 0;
    momentaryHopCount = 0;

    shortTermRingBuffer.fill(0.0);
    shortTermRingIdx = 0;
    shortTermHopCount = 0;

    const size_t estimatedHops = static_cast<size_t>(std::ceil(maxDurationSeconds / 0.100));
    recordedHopEnergies.assign(estimatedHops, 0.0);
    recordedHopCount = 0;

    recordedShortTermLoudness.assign(estimatedHops, -70.0);
    recordedShortTermCount = 0;

    reset();
}

void LoudnessMeter::reset() noexcept {
    sampleCounter = 0;
    accumEnergyL = accumEnergyR = 0.0;

    for (auto& kf : kFilters) {
        kf.reset();
    }

    momentaryRingBuffer.fill(0.0);
    momentaryRingIdx = 0;
    momentaryHopCount = 0;

    shortTermRingBuffer.fill(0.0);
    shortTermRingIdx = 0;
    shortTermHopCount = 0;

    recordedHopCount = 0;
    recordedShortTermCount = 0;

    atomicMomentary.store(-70.0f, std::memory_order_relaxed);
    atomicShortTerm.store(-70.0f, std::memory_order_relaxed);
    atomicMaxShortTerm.store(-70.0f, std::memory_order_relaxed);
    atomicIntegrated.store(-70.0f, std::memory_order_relaxed);
    atomicLRA.store(0.0f, std::memory_order_relaxed);
}

void LoudnessMeter::processHop(double energyL, double energyR) noexcept {
    const double hopMeanEnergy = energyL + energyR;

    // 1. Record hop energy for gated integration if within pre-allocated capacity
    if (recordedHopCount < recordedHopEnergies.size()) {
        recordedHopEnergies[recordedHopCount++] = hopMeanEnergy;
    }

    // 2. Update 400ms Momentary ring buffer (4 hops)
    momentaryRingBuffer[static_cast<size_t>(momentaryRingIdx)] = hopMeanEnergy;
    momentaryRingIdx = (momentaryRingIdx + 1) % 4;
    if (momentaryHopCount < 4) ++momentaryHopCount;

    double momentarySum = 0.0;
    for (int i = 0; i < momentaryHopCount; ++i) {
        momentarySum += momentaryRingBuffer[static_cast<size_t>(i)];
    }
    const double momentaryMean = momentarySum / momentaryHopCount;
    const double momLufs = (momentaryMean > 1.0e-12) ? (-0.691 + 10.0 * std::log10(momentaryMean)) : -70.0;
    atomicMomentary.store(static_cast<float>(momLufs), std::memory_order_relaxed);

    // 3. Update 3000ms Short-Term ring buffer (30 hops)
    shortTermRingBuffer[static_cast<size_t>(shortTermRingIdx)] = hopMeanEnergy;
    shortTermRingIdx = (shortTermRingIdx + 1) % 30;
    if (shortTermHopCount < 30) ++shortTermHopCount;

    double stSum = 0.0;
    for (int i = 0; i < shortTermHopCount; ++i) {
        stSum += shortTermRingBuffer[static_cast<size_t>(i)];
    }
    const double stMean = stSum / shortTermHopCount;
    const double stLufs = (stMean > 1.0e-12) ? (-0.691 + 10.0 * std::log10(stMean)) : -70.0;
    atomicShortTerm.store(static_cast<float>(stLufs), std::memory_order_relaxed);

    // Track max short-term
    float currentMax = atomicMaxShortTerm.load(std::memory_order_relaxed);
    if (static_cast<float>(stLufs) > currentMax) {
        atomicMaxShortTerm.store(static_cast<float>(stLufs), std::memory_order_relaxed);
    }

    // Record short-term history for LRA
    if (shortTermHopCount >= 30 && recordedShortTermCount < recordedShortTermLoudness.size()) {
        recordedShortTermLoudness[recordedShortTermCount++] = stLufs;
    }
}

void LoudnessMeter::processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept {
    const float* inL = channelData[0];
    const float* inR = (numChannels > 1) ? channelData[1] : channelData[0];

    for (int i = 0; i < numSamples; ++i) {
        const double yL = kFilters[0].processSample(static_cast<double>(inL[i]));
        const double yR = kFilters[1].processSample(static_cast<double>(inR[i]));

        accumEnergyL += yL * yL;
        accumEnergyR += yR * yR;

        if (++sampleCounter >= hopSamples100ms) {
            const double normL = accumEnergyL / static_cast<double>(hopSamples100ms);
            const double normR = accumEnergyR / static_cast<double>(hopSamples100ms);
            processHop(normL, normR);

            sampleCounter = 0;
            accumEnergyL = 0.0;
            accumEnergyR = 0.0;
        }
    }
}

LoudnessResult LoudnessMeter::calculateFinalTelemetry(float currentTruePeakdBTP) const {
    LoudnessResult result;
    result.momentaryLUFS = atomicMomentary.load(std::memory_order_relaxed);
    result.shortTermLUFS = atomicShortTerm.load(std::memory_order_relaxed);
    result.maxShortTermLUFS = atomicMaxShortTerm.load(std::memory_order_relaxed);

    // 1. Gated Integrated Loudness (ITU-R BS.1770-4)
    if (recordedHopCount >= 4) {
        const size_t numBlocks = recordedHopCount - 3;
        std::vector<double> blockEnergies;
        blockEnergies.reserve(numBlocks);

        for (size_t j = 0; j < numBlocks; ++j) {
            const double z = 0.25 * (recordedHopEnergies[j] +
                                     recordedHopEnergies[j + 1] +
                                     recordedHopEnergies[j + 2] +
                                     recordedHopEnergies[j + 3]);
            blockEnergies.push_back(z);
        }

        // Absolute threshold at -70.0 LUFS
        const double absThreshold = std::pow(10.0, (-70.0 + 0.691) / 10.0);
        double sumAbs = 0.0;
        int countAbs = 0;
        for (double z : blockEnergies) {
            if (z > absThreshold) {
                sumAbs += z;
                ++countAbs;
            }
        }

        if (countAbs > 0) {
            // Relative threshold = Mean of surviving absolute blocks - 10 LU
            const double relThreshold = (sumAbs / countAbs) * 0.1; // -10 dB = 0.1
            double sumRel = 0.0;
            int countRel = 0;
            for (double z : blockEnergies) {
                if (z > relThreshold) {
                    sumRel += z;
                    ++countRel;
                }
            }

            if (countRel > 0) {
                result.integratedLUFS = static_cast<float>(-0.691 + 10.0 * std::log10(sumRel / countRel));
            }
        }
    }

    // 2. Loudness Range (LRA, EBU Tech 3342)
    if (recordedShortTermCount > 0) {
        std::vector<double> lraValues;
        lraValues.reserve(recordedShortTermCount);

        // Absolute gate: -70.0 LUFS
        for (size_t i = 0; i < recordedShortTermCount; ++i) {
            const double val = recordedShortTermLoudness[i];
            if (val > -70.0) {
                lraValues.push_back(val);
            }
        }

        if (!lraValues.empty()) {
            // Compute power-mean of absolute-gated values
            double powerSum = 0.0;
            for (double val : lraValues) {
                powerSum += std::pow(10.0, (val + 0.691) / 10.0);
            }
            const double meanPower = powerSum / static_cast<double>(lraValues.size());
            const double relThresholdLRA = (-0.691 + 10.0 * std::log10(meanPower)) - 20.0; // -20 LU relative gate

            std::vector<double> survivingLRA;
            survivingLRA.reserve(lraValues.size());
            for (double val : lraValues) {
                if (val > relThresholdLRA) {
                    survivingLRA.push_back(val);
                }
            }

            if (survivingLRA.size() >= 2) {
                std::sort(survivingLRA.begin(), survivingLRA.end());
                const double maxIdx = static_cast<double>(survivingLRA.size() - 1);
                const size_t idx10 = static_cast<size_t>(std::round(0.10 * maxIdx));
                const size_t idx95 = static_cast<size_t>(std::round(0.95 * maxIdx));
                result.loudnessRangeLU = static_cast<float>(survivingLRA[idx95] - survivingLRA[idx10]);
            }
        }
    }

    // 3. Dynamic Ratios: Global PLR and Localized PSR
    result.plrLU = currentTruePeakdBTP - result.integratedLUFS;

    // Minimum PSR across 30-second moving windows (300 hops)
    if (recordedShortTermCount > 0) {
        double maxSTOverall = -70.0;
        for (size_t i = 0; i < recordedShortTermCount; ++i) {
            maxSTOverall = std::max(maxSTOverall, recordedShortTermLoudness[i]);
        }
        result.minPsrLU = currentTruePeakdBTP - static_cast<float>(maxSTOverall);
    } else {
        result.minPsrLU = currentTruePeakdBTP - result.shortTermLUFS;
    }

    return result;
}

} // namespace dsp
