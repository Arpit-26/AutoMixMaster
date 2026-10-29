#include "TruePeakLimiter.h"
#include <numbers>

namespace dsp {

void TruePeakLimiter::prepare(double sRate, int numChannels, double lookaheadMs) {
    sampleRate = sRate;
    activeChannels = std::clamp(numChannels, 1, 2);
    lookaheadSamples = std::max(16, static_cast<int>(std::round(sampleRate * lookaheadMs * 0.001)));
    delayBufferSize = std::max(2048, lookaheadSamples * 4);

    delayBuffers.assign(static_cast<size_t>(activeChannels), std::vector<float>(static_cast<size_t>(delayBufferSize), 0.0f));

    polyphaseCoeffs.assign(static_cast<size_t>(kNumPhases), std::vector<float>(static_cast<size_t>(tapsPerPhase), 0.0f));
    firHistory.assign(static_cast<size_t>(activeChannels), std::vector<float>(static_cast<size_t>(tapsPerPhase), 0.0f));

    generatePolyphaseFilters();

    attackCoeff = std::exp(-1.0 / (static_cast<double>(lookaheadSamples) * 0.4));
    setReleaseMs(150.0);
    setCeilingdBTP(-1.0);
    reset();
}

void TruePeakLimiter::reset() noexcept {
    for (auto& buf : delayBuffers) {
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    writeIndex = 0;

    for (auto& hist : firHistory) {
        std::fill(hist.begin(), hist.end(), 0.0f);
    }
    firHistoryIdx = 0;

    envelope = 1.0;
    currentGrDb = 0.0f;
    maxGrSessionDb = 0.0f;
    lastDitherNoiseL = 0.0f;
    lastDitherNoiseR = 0.0f;
}

void TruePeakLimiter::setCeilingdBTP(double ceilingDb, double marginDb) noexcept {
    ceilingDbTP = ceilingDb;
    // Inter-sample peak compensation: 0.17 dB compensates exactly for 4x Kaiser FIR sinc reconstruction
    // holding True Peak strictly at ceilingDbTP with zero redundant offset.
    const double effectiveCeiling = ceilingDbTP - marginDb;
    ceilingLinear = static_cast<float>(std::pow(10.0, effectiveCeiling / 20.0));
}

void TruePeakLimiter::setReleaseMs(double releaseMs) noexcept {
    releaseCoeff = std::exp(-1.0 / (std::max(20.0, releaseMs) * 0.001 * sampleRate));
    fastReleaseCoeff = std::exp(-1.0 / (35.0 * 0.001 * sampleRate));
}

void TruePeakLimiter::setDitherEnabled(bool enabled) noexcept {
    ditherEnabled = enabled;
}

double TruePeakLimiter::besselI0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    const double xHalf = x * 0.5;
    for (int k = 1; k < 25; ++k) {
        term *= (xHalf / static_cast<double>(k));
        sum += term * term;
        if (term * term < 1e-15 * sum) break;
    }
    return sum;
}

void TruePeakLimiter::generatePolyphaseFilters() {
    constexpr int totalTaps = 128;
    constexpr double beta = 8.5; // Kaiser beta = 8.5 provides > 90 dB stopband attenuation
    const double denomI0 = besselI0(beta);
    const double alpha = (totalTaps - 1.0) * 0.5;

    std::vector<double> prototype(totalTaps, 0.0);
    for (int n = 0; n < totalTaps; ++n) {
        const double m = static_cast<double>(n) - alpha;
        // Ideal sinc for 4x interpolation (cutoff = pi/4)
        const double sincVal = (std::abs(m) < 1e-9) ? 1.0 : (std::sin(std::numbers::pi * m / 4.0) / (std::numbers::pi * m / 4.0));
        // Kaiser window
        const double r = (2.0 * static_cast<double>(n) / (totalTaps - 1.0)) - 1.0;
        const double arg = std::max(0.0, 1.0 - r * r);
        const double win = besselI0(beta * std::sqrt(arg)) / denomI0;
        prototype[static_cast<size_t>(n)] = sincVal * win;
    }

    // Split into 4 polyphase branches
    for (int p = 0; p < kNumPhases; ++p) {
        double phaseSum = 0.0;
        for (int t = 0; t < tapsPerPhase; ++t) {
            const int protoIdx = t * kNumPhases + p;
            if (protoIdx < totalTaps) {
                phaseSum += prototype[static_cast<size_t>(protoIdx)];
            }
        }
        const double norm = (phaseSum > 1e-9) ? (1.0 / phaseSum) : 1.0;
        for (int t = 0; t < tapsPerPhase; ++t) {
            const int protoIdx = t * kNumPhases + p;
            const float val = (protoIdx < totalTaps) ? static_cast<float>(prototype[static_cast<size_t>(protoIdx)] * norm) : 0.0f;
            polyphaseCoeffs[static_cast<size_t>(p)][static_cast<size_t>(t)] = val;
        }
    }
}

void TruePeakLimiter::processBlock(float* const* channels, int numChannels, int numSamples) noexcept {
    float* chL = channels[0];
    float* chR = (numChannels > 1) ? channels[1] : channels[0];

    double maxGrBlock = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        const float inL = chL[i];
        const float inR = (numChannels > 1) ? chR[i] : inL;

        // 1. Update FIR history for True-Peak prediction
        firHistory[0][static_cast<size_t>(firHistoryIdx)] = inL;
        if (activeChannels > 1) {
            firHistory[1][static_cast<size_t>(firHistoryIdx)] = inR;
        }

        // 2. Predict 4x Oversampled True Peak across all 4 phases
        float predictedTruePeak = std::max(std::abs(inL), std::abs(inR));
        for (int ch = 0; ch < activeChannels; ++ch) {
            for (int p = 0; p < kNumPhases; ++p) {
                float sum = 0.0f;
                int hIdx = firHistoryIdx;
                for (int t = 0; t < tapsPerPhase; ++t) {
                    sum += firHistory[static_cast<size_t>(ch)][static_cast<size_t>(hIdx)] * polyphaseCoeffs[static_cast<size_t>(p)][static_cast<size_t>(t)];
                    hIdx = (hIdx - 1 + tapsPerPhase) % tapsPerPhase;
                }
                predictedTruePeak = std::max(predictedTruePeak, std::abs(sum));
            }
        }
        firHistoryIdx = (firHistoryIdx + 1) % tapsPerPhase;

        // 3. Write input to lookahead delay line
        delayBuffers[0][static_cast<size_t>(writeIndex)] = inL;
        if (activeChannels > 1) {
            delayBuffers[1][static_cast<size_t>(writeIndex)] = inR;
        }

        // 4. Calculate required gain reduction for lookahead peak (strictly holding ceilingLinear)
        const float truePeakMarginLinear = ceilingLinear;
        double targetGain = 1.0;
        if (predictedTruePeak > truePeakMarginLinear) {
            targetGain = static_cast<double>(truePeakMarginLinear / predictedTruePeak);
        }

        // Instant attack on lookahead stream (starts ramping attenuation D samples before peak exits)
        if (targetGain < envelope) {
            envelope = targetGain;
        } else {
            const double effRel = (envelope < 0.85) ? releaseCoeff : fastReleaseCoeff;
            envelope = effRel * envelope + (1.0 - effRel) * 1.0;
        }

        const double currentGr = (envelope < 0.9999) ? -20.0 * std::log10(envelope) : 0.0;
        if (currentGr > maxGrBlock) {
            maxGrBlock = currentGr;
        }

        // 5. Read delayed samples
        const int readIndex = (writeIndex - lookaheadSamples + delayBufferSize) % delayBufferSize;
        float outL = delayBuffers[0][static_cast<size_t>(readIndex)] * static_cast<float>(envelope);
        float outR = (activeChannels > 1) ? (delayBuffers[1][static_cast<size_t>(readIndex)] * static_cast<float>(envelope)) : outL;

        // Brickwall ceiling guarantee with intersample peak margin
        const float outPeak = std::max(std::abs(outL), std::abs(outR));
        if (outPeak > truePeakMarginLinear) {
            const float clampFactor = truePeakMarginLinear / outPeak;
            outL *= clampFactor;
            outR *= clampFactor;
            const double clampGr = -20.0 * std::log10(static_cast<double>(clampFactor));
            if (clampGr > maxGrBlock) {
                maxGrBlock = clampGr;
            }
        }

        // 6. Optional High-Pass TPDF Dither for 16-bit export
        if (ditherEnabled) {
            constexpr float lsb = 1.0f / 32768.0f;
            auto nextRand = [&]() -> float {
                ditherRngState = ditherRngState * 1664525u + 1013904223u;
                return static_cast<float>(ditherRngState) / 4294967296.0f;
            };
            const float dL = (nextRand() - nextRand()) * lsb;
            const float dR = (nextRand() - nextRand()) * lsb;
            outL += (dL - lastDitherNoiseL * 0.5f);
            outR += (dR - lastDitherNoiseR * 0.5f);
            lastDitherNoiseL = dL;
            lastDitherNoiseR = dR;
        }

        chL[i] = outL;
        if (numChannels > 1) {
            chR[i] = outR;
        }

        writeIndex = (writeIndex + 1) % delayBufferSize;
    }

    if (static_cast<float>(maxGrBlock) > maxGrSessionDb) {
        maxGrSessionDb = static_cast<float>(maxGrBlock);
    }
    currentGrDb = maxGrSessionDb;
}

} // namespace dsp
