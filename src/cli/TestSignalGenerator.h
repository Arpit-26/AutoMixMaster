#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../dsp/LoudnessMeter.h"
#include <numbers>
#include <random>
#include <cmath>
#include <array>
#include <algorithm>

namespace cli {

class TestSignalGenerator {
public:
    enum class SignalType {
        CalibratedMasterTrack,
        Sine1kHz,
        PinkNoise,
        OutPhaseBass60Hz,
        DynamicDrumsBurst
    };

    /**
     * Calibrate an audio buffer to exact ITU-R BS.1770-4 Integrated Loudness.
     */
    static void calibrateToTargetLUFS(juce::AudioBuffer<float>& buffer, double sampleRate, double targetLUFS) {
        dsp::LoudnessMeter meter;
        meter.prepare(sampleRate, 1024);

        const int numSamples = buffer.getNumSamples();
        const int stepSize = 1024;
        for (int i = 0; i < numSamples; i += stepSize) {
            const int count = std::min(stepSize, numSamples - i);
            juce::AudioBuffer<float> temp(buffer.getArrayOfWritePointers(), 2, i, count);
            meter.processBlock(temp.getArrayOfReadPointers(), 2, count);
        }

        const auto finalLoudness = meter.calculateFinalTelemetry(0.0f);
        const double measuredLUFS = finalLoudness.integratedLUFS;
        if (measuredLUFS > -70.0 && measuredLUFS < 10.0) {
            const double deltaDb = targetLUFS - measuredLUFS;
            const float gainFactor = static_cast<float>(std::pow(10.0, deltaDb / 20.0));
            buffer.applyGain(gainFactor);
        }
    }

    /**
     * Generates a stereo audio buffer of durationSeconds at sampleRate.
     * targetDbFS specifies the target LUFS when calibrated.
     */
    static juce::AudioBuffer<float> generate(SignalType type,
                                             double sampleRate,
                                             double durationSeconds,
                                             float targetDbFS = -14.0f) {
        const int numSamples = static_cast<int>(std::round(sampleRate * durationSeconds));
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        const float linearAmp = std::pow(10.0f, targetDbFS / 20.0f);

        switch (type) {
            case SignalType::CalibratedMasterTrack: {
                // 10-second master-track simulation with mono kick/sub-bass (<120 Hz),
                // mid-range musical harmonic pads (positive correlation ~0.85),
                // and commercial -4.5 dB/oct spectral tilt.
                std::mt19937 rng(1337);
                std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

                std::array<float, 5> pinkState = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
                auto nextPink = [&]() -> float {
                    const float white = noiseDist(rng);
                    pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
                    pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
                    pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
                    pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
                    pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
                    return (pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3] + pinkState[4] + white * 0.5362f) * 0.15f;
                };

                const int beatInterval = static_cast<int>(std::round(sampleRate * 0.5)); // 120 BPM
                double bassPhase = 0.0;
                double padPhase1 = 0.0;
                double padPhase2 = 0.0;

                for (int i = 0; i < numSamples; ++i) {
                    // 1. Mono Sub-Bass & Kick (<120 Hz) -> correlation = 1.000, 0 dB mono loss
                    const int posInBeat = i % beatInterval;
                    float kickVal = 0.0f;
                    if (posInBeat < static_cast<int>(sampleRate * 0.12)) {
                        const double env = std::exp(-static_cast<double>(posInBeat) / (sampleRate * 0.035));
                        const double kickFreq = 50.0 + 60.0 * env;
                        kickVal = 0.65f * static_cast<float>(std::sin(2.0 * std::numbers::pi * kickFreq * (posInBeat / sampleRate)) * env);
                    }

                    bassPhase += 2.0 * std::numbers::pi * 55.0 / sampleRate;
                    if (bassPhase >= 2.0 * std::numbers::pi) bassPhase -= 2.0 * std::numbers::pi;
                    const float subBassVal = 0.22f * static_cast<float>(std::sin(bassPhase));
                    const float monoLow = kickVal + subBassVal;

                    // 2. Stereo Musical Mids (300 Hz - 2 kHz)
                    padPhase1 += 2.0 * std::numbers::pi * 329.63 / sampleRate; // E4
                    padPhase2 += 2.0 * std::numbers::pi * 440.00 / sampleRate; // A4
                    if (padPhase1 >= 2.0 * std::numbers::pi) padPhase1 -= 2.0 * std::numbers::pi;
                    if (padPhase2 >= 2.0 * std::numbers::pi) padPhase2 -= 2.0 * std::numbers::pi;

                    const float midPart  = 0.22f * static_cast<float>(std::sin(padPhase1) + std::sin(padPhase2));
                    const float sidePart = 0.06f * static_cast<float>(std::sin(padPhase1) - std::sin(padPhase2));

                    // 3. Pink Noise Highs (>2 kHz)
                    const float pink = 0.15f * nextPink();
                    const float pinkSpread = 0.03f * noiseDist(rng);

                    const float outL = monoLow + (midPart + sidePart) + (pink + pinkSpread);
                    const float outR = monoLow + (midPart - sidePart) + (pink - pinkSpread);

                    buffer.setSample(0, i, outL);
                    buffer.setSample(1, i, outR);
                }

                // Normalization pass
                calibrateToTargetLUFS(buffer, sampleRate, static_cast<double>(targetDbFS));
                break;
            }

            case SignalType::Sine1kHz: {
                constexpr double freq = 1000.0;
                const double phaseInc = 2.0 * std::numbers::pi * freq / sampleRate;
                // ITU-R BS.1770: 0 dBFS peak stereo sine = -0.691 LUFS
                // targetLUFS = -0.691 + 20*log10(amp) => amp = 10^((targetLUFS + 0.691) / 20)
                const float amp = static_cast<float>(std::pow(10.0, (targetDbFS + 0.691) / 20.0));
                double phase = 0.0;
                for (int i = 0; i < numSamples; ++i) {
                    const float val = amp * static_cast<float>(std::sin(phase));
                    buffer.setSample(0, i, val);
                    buffer.setSample(1, i, val);
                    phase += phaseInc;
                    if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
                }
                break;
            }

            case SignalType::PinkNoise: {
                std::mt19937 rng(1337);
                std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

                std::array<float, 5> pinkStateL = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
                std::array<float, 5> pinkStateR = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

                for (int i = 0; i < numSamples; ++i) {
                    auto updatePink = [&](std::array<float, 5>& state) -> float {
                        const float white = dist(rng);
                        state[0] = 0.99886f * state[0] + white * 0.0555179f;
                        state[1] = 0.99332f * state[1] + white * 0.0750759f;
                        state[2] = 0.96900f * state[2] + white * 0.1538520f;
                        state[3] = 0.86650f * state[3] + white * 0.3104856f;
                        state[4] = 0.55000f * state[4] + white * 0.5329522f;
                        return (state[0] + state[1] + state[2] + state[3] + state[4] + white * 0.5362f) * 0.15f;
                    };

                    const float pL = updatePink(pinkStateL);
                    const float pR = updatePink(pinkStateR);
                    // 70% mono + 30% stereo pink noise
                    const float mono = 0.7f * pL;
                    buffer.setSample(0, i, mono + 0.3f * pL);
                    buffer.setSample(1, i, mono + 0.3f * pR);
                }
                calibrateToTargetLUFS(buffer, sampleRate, static_cast<double>(targetDbFS));
                break;
            }

            case SignalType::OutPhaseBass60Hz: {
                constexpr double freq = 60.0;
                const double phaseInc = 2.0 * std::numbers::pi * freq / sampleRate;
                double phase = 0.0;
                for (int i = 0; i < numSamples; ++i) {
                    const float val = linearAmp * static_cast<float>(std::sin(phase));
                    buffer.setSample(0, i, val);
                    buffer.setSample(1, i, -val);
                    phase += phaseInc;
                    if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
                }
                break;
            }

            case SignalType::DynamicDrumsBurst: {
                std::mt19937 rng(42);
                std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                const int beatInterval = static_cast<int>(std::round(sampleRate * 0.5));

                for (int i = 0; i < numSamples; ++i) {
                    const int posInBeat = i % beatInterval;
                    float val = 0.0f;
                    if (posInBeat < static_cast<int>(sampleRate * 0.08)) {
                        const double env = std::exp(-static_cast<double>(posInBeat) / (sampleRate * 0.02));
                        const double kickFreq = 120.0 * env + 45.0;
                        const double phase = 2.0 * std::numbers::pi * kickFreq * (posInBeat / sampleRate);
                        val = linearAmp * static_cast<float>(std::sin(phase) * env);
                    } else {
                        val = linearAmp * 0.05f * dist(rng);
                    }
                    buffer.setSample(0, i, val);
                    buffer.setSample(1, i, val);
                }
                calibrateToTargetLUFS(buffer, sampleRate, static_cast<double>(targetDbFS));
                break;
            }
        }

        return buffer;
    }
};

} // namespace cli
