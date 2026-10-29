#include "SpatialAnalyzer.h"
#include <algorithm>

namespace dsp {

void SpatialAnalyzer::prepare(double sRate, int /*maxBlockSize*/) {
    sampleRate = sRate;
    updateInterval = std::max(256, static_cast<int>(std::round(sampleRate * 0.050))); // ~50ms window

    xover120L.prepare(sampleRate, 120.0);
    xover120R.prepare(sampleRate, 120.0);

    xover1000L.prepare(sampleRate, 1000.0);
    xover1000R.prepare(sampleRate, 1000.0);

    xover5000L.prepare(sampleRate, 5000.0);
    xover5000R.prepare(sampleRate, 5000.0);

    reset();
}

void SpatialAnalyzer::reset() noexcept {
    xover120L.reset();
    xover120R.reset();
    xover1000L.reset();
    xover1000R.reset();
    xover5000L.reset();
    xover5000R.reset();

    for (auto& b : bandAccums) {
        b.dotLR = b.sumLL = b.sumRR = 0.0;
    }
    globalAccum.dotLR = globalAccum.sumLL = globalAccum.sumRR = 0.0;
    subStereoEnergy = 0.0;
    subMonoEnergy = 0.0;
    sampleCounter = 0;

    for (auto& corr : atomicBandCorrelation) {
        corr.store(1.0f, std::memory_order_relaxed);
    }
    atomicGlobalCorrelation.store(1.0f, std::memory_order_relaxed);
    atomicSubMonoLoss_dB.store(0.0f, std::memory_order_relaxed);
    atomicMonoIncompatible.store(false, std::memory_order_relaxed);
}

void SpatialAnalyzer::processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept {
    const float* inL = channelData[0];
    const float* inR = (numChannels > 1) ? channelData[1] : channelData[0];

    for (int i = 0; i < numSamples; ++i) {
        const double xL = static_cast<double>(inL[i]);
        const double xR = static_cast<double>(inR[i]);

        // Global accumulation
        globalAccum.dotLR += xL * xR;
        globalAccum.sumLL += xL * xL;
        globalAccum.sumRR += xR * xR;

        // Split 1: 120 Hz
        double band1L = 0.0, above120L = 0.0;
        double band1R = 0.0, above120R = 0.0;
        xover120L.processSample(xL, band1L, above120L);
        xover120R.processSample(xR, band1R, above120R);

        // Split 2: 1000 Hz
        double band2L = 0.0, above1000L = 0.0;
        double band2R = 0.0, above1000R = 0.0;
        xover1000L.processSample(above120L, band2L, above1000L);
        xover1000R.processSample(above120R, band2R, above1000R);

        // Split 3: 5000 Hz
        double band3L = 0.0, band4L = 0.0;
        double band3R = 0.0, band4R = 0.0;
        xover5000L.processSample(above1000L, band3L, band4L);
        xover5000R.processSample(above1000R, band3R, band4R);

        // Accumulate band correlation products
        const std::array<double, 4> bandsL = { band1L, band2L, band3L, band4L };
        const std::array<double, 4> bandsR = { band1R, band2R, band3R, band4R };

        for (size_t b = 0; b < 4; ++b) {
            bandAccums[b].dotLR += bandsL[b] * bandsR[b];
            bandAccums[b].sumLL += bandsL[b] * bandsL[b];
            bandAccums[b].sumRR += bandsR[b] * bandsR[b];
        }

        // Sub-bass (<120 Hz) stereo vs mono energy
        subStereoEnergy += (band1L * band1L + band1R * band1R);
        const double subMono = 0.5 * (band1L + band1R);
        subMonoEnergy += 2.0 * (subMono * subMono);

        if (++sampleCounter >= updateInterval) {
            // Compute correlation for each of the 4 bands
            for (size_t b = 0; b < 4; ++b) {
                const double denom = std::sqrt(bandAccums[b].sumLL * bandAccums[b].sumRR);
                const float corr = (denom > 1.0e-12) ? static_cast<float>(bandAccums[b].dotLR / denom) : 1.0f;
                atomicBandCorrelation[b].store(corr, std::memory_order_relaxed);

                bandAccums[b].dotLR = 0.0;
                bandAccums[b].sumLL = 0.0;
                bandAccums[b].sumRR = 0.0;
            }

            // Global correlation
            const double globalDenom = std::sqrt(globalAccum.sumLL * globalAccum.sumRR);
            const float globalCorr = (globalDenom > 1.0e-12) ? static_cast<float>(globalAccum.dotLR / globalDenom) : 1.0f;
            atomicGlobalCorrelation.store(globalCorr, std::memory_order_relaxed);
            globalAccum.dotLR = globalAccum.sumLL = globalAccum.sumRR = 0.0;

            // Sub-bass mono-sum loss in dB
            if (subMonoEnergy > 1.0e-12) {
                const double loss_dB = 10.0 * std::log10(std::max(1.0, subStereoEnergy / subMonoEnergy));
                const float lossVal = static_cast<float>(loss_dB);
                atomicSubMonoLoss_dB.store(lossVal, std::memory_order_relaxed);
                atomicMonoIncompatible.store(lossVal > 1.0f, std::memory_order_relaxed);
            } else {
                atomicSubMonoLoss_dB.store(0.0f, std::memory_order_relaxed);
                atomicMonoIncompatible.store(false, std::memory_order_relaxed);
            }

            subStereoEnergy = 0.0;
            subMonoEnergy = 0.0;
            sampleCounter = 0;
        }
    }
}

SpatialMetrics SpatialAnalyzer::getMetrics() const noexcept {
    SpatialMetrics metrics;
    for (size_t b = 0; b < 4; ++b) {
        metrics.bandCorrelation[b] = atomicBandCorrelation[b].load(std::memory_order_relaxed);
    }
    metrics.globalCorrelation = atomicGlobalCorrelation.load(std::memory_order_relaxed);
    metrics.subMonoSumLoss_dB = atomicSubMonoLoss_dB.load(std::memory_order_relaxed);
    metrics.monoIncompatible = atomicMonoIncompatible.load(std::memory_order_relaxed);
    return metrics;
}

} // namespace dsp
