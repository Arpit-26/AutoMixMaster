#include "TruePeakMeter.h"
#include <numbers>
#include <numeric>

namespace dsp {

double TruePeakMeter::besselI0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    const double xHalf = x * 0.5;
    for (int k = 1; k < 30; ++k) {
        term *= (xHalf / k);
        sum += term * term;
        if (term * term < 1.0e-15 * sum)
            break;
    }
    return sum;
}

void TruePeakMeter::generatePolyphaseFilters(TruePeakMode mode) {
    currentMode = mode;
    const int totalTaps = (mode == TruePeakMode::Mastering_Kaiser_128Tap) ? 128 : 48;
    tapsPerPhase = totalTaps / 4;
    const double beta = (mode == TruePeakMode::Mastering_Kaiser_128Tap) ? 8.5 : 5.0;

    std::vector<double> prototype(static_cast<size_t>(totalTaps), 0.0);
    const double midPoint = (totalTaps - 1) * 0.5;
    const double i0Beta = besselI0(beta);
    constexpr double cutoff = 0.25; // 4x Nyquist cutoff

    for (int i = 0; i < totalTaps; ++i) {
        const double diff = static_cast<double>(i) - midPoint;
        double sincVal = 1.0;
        if (std::abs(diff) > 1.0e-9) {
            const double arg = std::numbers::pi * cutoff * diff;
            sincVal = std::sin(arg) / (std::numbers::pi * diff);
        } else {
            sincVal = cutoff;
        }

        // Kaiser window
        const double normPos = (2.0 * static_cast<double>(i) / (totalTaps - 1)) - 1.0;
        const double kaiserArg = beta * std::sqrt(std::max(0.0, 1.0 - normPos * normPos));
        const double winVal = besselI0(kaiserArg) / i0Beta;

        prototype[static_cast<size_t>(i)] = 4.0 * sincVal * winVal;
    }

    // Polyphase decomposition: branch p gets prototype[4*k + p]
    for (size_t p = 0; p < 4; ++p) {
        polyphaseBranches[p].assign(static_cast<size_t>(tapsPerPhase), 0.0f);
        double branchSum = 0.0;
        for (int k = 0; k < tapsPerPhase; ++k) {
            const size_t protoIdx = static_cast<size_t>(4 * k + static_cast<int>(p));
            polyphaseBranches[p][static_cast<size_t>(k)] = static_cast<float>(prototype[protoIdx]);
            branchSum += prototype[protoIdx];
        }

        // Normalize DC gain of each branch to 1.0 to guarantee zero amplitude distortion
        if (std::abs(branchSum) > 1.0e-9) {
            const float normFactor = static_cast<float>(1.0 / branchSum);
            for (int k = 0; k < tapsPerPhase; ++k) {
                polyphaseBranches[p][static_cast<size_t>(k)] *= normFactor;
            }
        }
    }
}

void TruePeakMeter::prepare(double /*sampleRate*/, int numChannels, TruePeakMode mode) {
    activeChannels = std::max(1, numChannels);
    generatePolyphaseFilters(mode);

    channelDelayLines.assign(static_cast<size_t>(activeChannels),
                             std::vector<float>(static_cast<size_t>(tapsPerPhase), 0.0f));
    delayLineIndices.assign(static_cast<size_t>(activeChannels), 0);
    resetPeak();
}

void TruePeakMeter::reset() noexcept {
    for (auto& dl : channelDelayLines) {
        std::fill(dl.begin(), dl.end(), 0.0f);
    }
    std::fill(delayLineIndices.begin(), delayLineIndices.end(), 0);
    resetPeak();
}

void TruePeakMeter::processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept {
    const int channelsToProcess = std::min(numChannels, activeChannels);
    float localMax = peakValue.load(std::memory_order_relaxed);

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        const float* in = channelData[ch];
        auto& dl = channelDelayLines[static_cast<size_t>(ch)];
        int writePos = delayLineIndices[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            dl[static_cast<size_t>(writePos)] = in[i];

            // 4 Polyphase branches evaluated per input sample
            for (size_t p = 0; p < 4; ++p) {
                const auto& branch = polyphaseBranches[p];
                float sampleInterp = 0.0f;
                int readPos = writePos;

                for (int k = 0; k < tapsPerPhase; ++k) {
                    sampleInterp += branch[static_cast<size_t>(k)] * dl[static_cast<size_t>(readPos)];
                    if (--readPos < 0) {
                        readPos += tapsPerPhase;
                    }
                }

                localMax = std::max(localMax, std::abs(sampleInterp));
            }

            if (++writePos >= tapsPerPhase) {
                writePos = 0;
            }
        }

        delayLineIndices[static_cast<size_t>(ch)] = writePos;
    }

    float prev = peakValue.load(std::memory_order_relaxed);
    while (localMax > prev && !peakValue.compare_exchange_weak(prev, localMax, std::memory_order_relaxed)) {
        // Spin until updated
    }
}

} // namespace dsp
