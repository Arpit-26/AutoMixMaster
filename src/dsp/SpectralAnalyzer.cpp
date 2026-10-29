#include "SpectralAnalyzer.h"
#include <algorithm>
#include <numeric>

namespace dsp {

void SpectralAnalyzer::prepare(double sRate, int /*maxBlockSize*/) {
    sampleRate = sRate;
    fifoBuffer.assign(static_cast<size_t>(kFifoSize), 0.0f);
    fifoWritePos = 0;
    samplesSinceLastFFT = 0;
    fftHopSize = kFFTSize / 4; // 75% overlap = 1024 samples

    fftTimeData.assign(static_cast<size_t>(kFFTSize * 2), 0.0f);
    fftFreqData.assign(static_cast<size_t>(kFFTSize / 2 + 1), 0.0f);
    ltasAccumulator.assign(static_cast<size_t>(kFFTSize / 2 + 1), 0.0);
    ltasFrameCount = 0;

    lowBandFilterL.prepare(sampleRate, 250.0);
    lowBandFilterR.prepare(sampleRate, 250.0);
    lowBandSumSq = 0.0;
    lowBandPeak = 0.0;
    lowBandSampleCount = 0;

    reset();
}

void SpectralAnalyzer::reset() noexcept {
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    fifoWritePos = 0;
    samplesSinceLastFFT = 0;

    std::fill(ltasAccumulator.begin(), ltasAccumulator.end(), 0.0);
    ltasFrameCount = 0;

    lowBandFilterL.reset();
    lowBandFilterR.reset();
    lowBandSumSq = 0.0;
    lowBandPeak = 0.0;
    lowBandSampleCount = 0;

    atomicBass_dB.store(0.0f, std::memory_order_relaxed);
    atomicLowMids_dB.store(0.0f, std::memory_order_relaxed);
    atomicHighMids_dB.store(0.0f, std::memory_order_relaxed);
    atomicTreble_dB.store(0.0f, std::memory_order_relaxed);
    atomicSpectralTilt.store(-4.5f, std::memory_order_relaxed);
    atomicLowBandCrest_dB.store(0.0f, std::memory_order_relaxed);
}

void SpectralAnalyzer::processFFTFrame() noexcept {
    // 1. Read last kFFTSize samples from circular ring buffer
    int readIdx = (fifoWritePos - kFFTSize + kFifoSize) % kFifoSize;
    for (int i = 0; i < kFFTSize; ++i) {
        fftTimeData[static_cast<size_t>(i)] = fifoBuffer[static_cast<size_t>(readIdx)];
        readIdx = (readIdx + 1) % kFifoSize;
    }
    std::fill(fftTimeData.begin() + kFFTSize, fftTimeData.end(), 0.0f);

    // 2. Window and execute forward FFT
    window.multiplyWithWindowingTable(fftTimeData.data(), static_cast<size_t>(kFFTSize));
    fft.performRealOnlyForwardTransform(fftTimeData.data());

    // 3. Compute bin energies and aggregate macro bands
    double bassSum = 0.0, lowMidsSum = 0.0, highMidsSum = 0.0, trebleSum = 0.0;
    const double binHz = sampleRate / static_cast<double>(kFFTSize);
    const int numBins = kFFTSize / 2;

    // Linear regression accumulators for spectral tilt (100 Hz to 10 kHz)
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;
    int regCount = 0;

    for (int k = 1; k < numBins; ++k) {
        const double re = static_cast<double>(fftTimeData[static_cast<size_t>(2 * k)]);
        const double im = static_cast<double>(fftTimeData[static_cast<size_t>(2 * k + 1)]);
        const double magSq = re * re + im * im;
        const double freqHz = k * binHz;

        ltasAccumulator[static_cast<size_t>(k)] += magSq;

        if (freqHz < 250.0) {
            bassSum += magSq;
        } else if (freqHz < 2000.0) {
            lowMidsSum += magSq;
        } else if (freqHz < 8000.0) {
            highMidsSum += magSq;
        } else {
            trebleSum += magSq;
        }

        if (freqHz >= 100.0 && freqHz <= 10000.0 && magSq > 1.0e-12) {
            const double logF = std::log10(freqHz);
            const double logMag = 10.0 * std::log10(magSq);
            sumX += logF;
            sumY += logMag;
            sumXY += logF * logMag;
            sumX2 += logF * logF;
            ++regCount;
        }
    }
    ++ltasFrameCount;

    // 4. Update macro band atomics (relative dB)
    auto toDb = [](double val) noexcept -> float {
        return (val > 1.0e-12) ? static_cast<float>(10.0 * std::log10(val)) : -100.0f;
    };
    atomicBass_dB.store(toDb(bassSum), std::memory_order_relaxed);
    atomicLowMids_dB.store(toDb(lowMidsSum), std::memory_order_relaxed);
    atomicHighMids_dB.store(toDb(highMidsSum), std::memory_order_relaxed);
    atomicTreble_dB.store(toDb(trebleSum), std::memory_order_relaxed);

    // 5. Update spectral tilt slope (dB/octave)
    if (regCount >= 5) {
        const double denom = (regCount * sumX2) - (sumX * sumX);
        if (std::abs(denom) > 1.0e-9) {
            const double alpha = ((regCount * sumXY) - (sumX * sumY)) / denom;
            const double slopeOctave = alpha * 0.3010299956639812; // log10(2)
            atomicSpectralTilt.store(static_cast<float>(slopeOctave), std::memory_order_relaxed);
        }
    }
}

void SpectralAnalyzer::processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept {
    const float* inL = channelData[0];
    const float* inR = (numChannels > 1) ? channelData[1] : channelData[0];

    for (int i = 0; i < numSamples; ++i) {
        // Mono mix into FFT circular ring buffer
        const float monoVal = 0.5f * (inL[i] + inR[i]);
        fifoBuffer[static_cast<size_t>(fifoWritePos)] = monoVal;
        fifoWritePos = (fifoWritePos + 1) % kFifoSize;

        if (++samplesSinceLastFFT >= fftHopSize) {
            processFFTFrame();
            samplesSinceLastFFT = 0;
        }

        // Low-band (<250 Hz) crest factor measurement
        double lowL = 0.0, highDummyL = 0.0;
        double lowR = 0.0, highDummyR = 0.0;
        lowBandFilterL.processSample(static_cast<double>(inL[i]), lowL, highDummyL);
        lowBandFilterR.processSample(static_cast<double>(inR[i]), lowR, highDummyR);

        const double lowMono = 0.5 * (lowL + lowR);
        lowBandSumSq += lowMono * lowMono;
        lowBandPeak = std::max(lowBandPeak, std::abs(lowMono));

        if (++lowBandSampleCount >= 4096) {
            const double rms = std::sqrt(lowBandSumSq / lowBandSampleCount);
            const double crest_dB = (rms > 1.0e-9) ? (20.0 * std::log10(lowBandPeak / rms)) : 0.0;
            atomicLowBandCrest_dB.store(static_cast<float>(crest_dB), std::memory_order_relaxed);

            lowBandSumSq = 0.0;
            lowBandPeak = 0.0;
            lowBandSampleCount = 0;
        }
    }
}

MacroBandScoring SpectralAnalyzer::getScoring() const noexcept {
    MacroBandScoring scoring;
    scoring.bass_dB = atomicBass_dB.load(std::memory_order_relaxed);
    scoring.lowMids_dB = atomicLowMids_dB.load(std::memory_order_relaxed);
    scoring.highMids_dB = atomicHighMids_dB.load(std::memory_order_relaxed);
    scoring.treble_dB = atomicTreble_dB.load(std::memory_order_relaxed);
    scoring.spectralTilt_dBoct = atomicSpectralTilt.load(std::memory_order_relaxed);
    scoring.lowBandCrest_dB = atomicLowBandCrest_dB.load(std::memory_order_relaxed);
    return scoring;
}

} // namespace dsp
