#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>

namespace dsp {

struct MacroBandScoring {
    float bass_dB          = 0.0f; // < 250 Hz
    float lowMids_dB       = 0.0f; // 250 Hz - 2 kHz
    float highMids_dB      = 0.0f; // 2 kHz - 8 kHz
    float treble_dB        = 0.0f; // > 8 kHz
    float spectralTilt_dBoct = -4.5f; // Estimated roll-off slope (dB/octave)
    float lowBandCrest_dB  = 0.0f; // Crest factor (< 250 Hz)
};

/**
 * Real-Time Safe Spectral & Crest Factor Analyzer.
 * Decouples host buffer sizes using pre-allocated circular ring buffer.
 * Performs 4096-point Hann-windowed FFT, computes 1/3-octave smoothed LTAS,
 * scores the 4 commercial macro bands against the -4.5 dB/oct tilt, and evaluates
 * low-band (<250 Hz) crest factor using 4th-order Linkwitz-Riley lowpass filtering.
 */
class SpectralAnalyzer {
public:
    static constexpr int kFFTOrder = 12; // 4096 points
    static constexpr int kFFTSize  = 1 << kFFTOrder;

    SpectralAnalyzer() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // Real-time audio processing: zero heap allocations
    void processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept;

    MacroBandScoring getScoring() const noexcept;

private:
    void processFFTFrame() noexcept;

    double sampleRate = 48000.0;
    juce::dsp::FFT fft { kFFTOrder };
    juce::dsp::WindowingFunction<float> window { kFFTSize, juce::dsp::WindowingFunction<float>::hann };

    // Decoupling circular ring buffer
    static constexpr int kFifoSize = 8192;
    std::vector<float> fifoBuffer;
    int fifoWritePos = 0;
    int samplesSinceLastFFT = 0;
    int fftHopSize = 1024; // 75% overlap

    // Pre-allocated FFT work buffers
    std::vector<float> fftTimeData;
    std::vector<float> fftFreqData;

    // Running Long-Term Average Spectrum (LTAS) accumulator
    std::vector<double> ltasAccumulator;
    int ltasFrameCount = 0;

    // Low-band (< 250 Hz) crest factor tracking
    LinkwitzRiley4thOrder lowBandFilterL;
    LinkwitzRiley4thOrder lowBandFilterR;
    double lowBandSumSq = 0.0;
    double lowBandPeak = 0.0;
    int lowBandSampleCount = 0;

    // Lock-free atomic readouts
    std::atomic<float> atomicBass_dB          { 0.0f };
    std::atomic<float> atomicLowMids_dB       { 0.0f };
    std::atomic<float> atomicHighMids_dB      { 0.0f };
    std::atomic<float> atomicTreble_dB        { 0.0f };
    std::atomic<float> atomicSpectralTilt     { -4.5f };
    std::atomic<float> atomicLowBandCrest_dB  { 0.0f };
};

} // namespace dsp
