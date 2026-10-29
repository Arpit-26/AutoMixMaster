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
        FlawedUnmasteredInput,
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
     * Calibrate an audio buffer to exact RMS level in dBFS.
     */
    static void calibrateToTargetRMS(juce::AudioBuffer<float>& buffer, float targetRmsDbFS) {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0 || numChannels == 0) return;

        double sumSq = 0.0;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float* p = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                const double s = static_cast<double>(p[i]);
                sumSq += s * s;
            }
        }
        const double meanSq = sumSq / static_cast<double>(numChannels * numSamples);
        const double measuredRms = std::sqrt(std::max(1.0e-9, meanSq));
        const double measuredRmsDb = 20.0 * std::log10(measuredRms);

        const double gainDeltaDb = static_cast<double>(targetRmsDbFS) - measuredRmsDb;
        const float gainLinear = static_cast<float>(std::pow(10.0, gainDeltaDb / 20.0));
        buffer.applyGain(gainLinear);
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

            case SignalType::FlawedUnmasteredInput: {
                // 10-second intentionally flawed unmastered mix:
                // - Quiet overall (-19.5 LUFS)
                // - Out-of-phase stereo sub-bass below 120 Hz causing > 2.5 dB mono-sum loss
                // - Loose sub-bass peaks (high <250 Hz crest factor) and a +5 dB mud buildup at 250 Hz
                // - Harsh resonant spike at 3.5 kHz
                // - Uncontrolled drum transients
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
                double subPhase = 0.0;
                double mudPhase = 0.0;
                double harshPhase = 0.0;

                for (int i = 0; i < numSamples; ++i) {
                    const int posInBeat = i % beatInterval;

                    // 1. Spiky uncontrolled transient drum bursts (fast 5ms decay)
                    float drumClick = 0.0f;
                    if (posInBeat < static_cast<int>(sampleRate * 0.06)) {
                        const double env = std::exp(-static_cast<double>(posInBeat) / (sampleRate * 0.012));
                        drumClick = 0.85f * static_cast<float>(env * noiseDist(rng));
                    }

                    // 2. Out-of-phase stereo 60 Hz sub-bass (180 deg inverted phase -> cancels completely in mono!)
                    subPhase += 2.0 * std::numbers::pi * 60.0 / sampleRate;
                    if (subPhase >= 2.0 * std::numbers::pi) subPhase -= 2.0 * std::numbers::pi;

                    // Dynamic 808 bursts with high crest factor (~14 dB)
                    float subEnv = 0.08f;
                    if (posInBeat < static_cast<int>(sampleRate * 0.3)) {
                        subEnv = 0.65f * static_cast<float>(std::exp(-static_cast<double>(posInBeat) / (sampleRate * 0.08)));
                    }
                    const float subTone = subEnv * static_cast<float>(std::sin(subPhase));
                    const float subL = subTone;
                    const float subR = -subTone; // 180 degrees out of phase! (L + R) = 0!

                    // 3. +5 dB mud buildup at 250 Hz (narrow stationary peak)
                    mudPhase += 2.0 * std::numbers::pi * 250.0 / sampleRate;
                    if (mudPhase >= 2.0 * std::numbers::pi) mudPhase -= 2.0 * std::numbers::pi;
                    const float mudVal = 0.18f * static_cast<float>(std::sin(mudPhase));

                    // 4. Harsh resonant spike at 3.5 kHz (piercing sibilance whistle)
                    harshPhase += 2.0 * std::numbers::pi * 3500.0 / sampleRate;
                    if (harshPhase >= 2.0 * std::numbers::pi) harshPhase -= 2.0 * std::numbers::pi;
                    const float harshVal = 0.15f * static_cast<float>(std::sin(harshPhase));

                    // 5. Baseline musical bed with -4.5 dB/oct commercial tilt
                    const float pinkBed = 0.16f * nextPink();
                    const float pinkSpread = 0.02f * noiseDist(rng);

                    const float outL = drumClick + subL + mudVal + harshVal + (pinkBed + pinkSpread);
                    const float outR = drumClick + subR + mudVal + harshVal + (pinkBed - pinkSpread);

                    buffer.setSample(0, i, outL);
                    buffer.setSample(1, i, outR);
                }

                // Calibrate unmastered mix to exactly -19.5 LUFS
                calibrateToTargetLUFS(buffer, sampleRate, -19.5);
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

    /**
     * Generates a punchy mono Kick stem at -6.0 dBFS RMS (un-gain-staged).
     * 120 BPM with 60 Hz punch and 4 kHz click snap.
     */
    static juce::AudioBuffer<float> generateKickStem(double sampleRate, double durationSeconds, float targetRmsDbFS = -6.0f) {
        const int numSamples = static_cast<int>(std::round(sampleRate * durationSeconds));
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);
        const int beatInterval = static_cast<int>(std::round(sampleRate * 0.5)); // 120 BPM

        for (int i = 0; i < numSamples; ++i) {
            const int posInBeat = i % beatInterval;
            float val = 0.0f;
            if (posInBeat < static_cast<int>(sampleRate * 0.15)) {
                const double t = static_cast<double>(posInBeat) / sampleRate;
                const double env = std::exp(-t / 0.040);
                const double phase = 2.0 * std::numbers::pi * 60.0 * t;
                val = 0.85f * static_cast<float>(std::sin(phase) * env);

                // Transient click snap (3ms at 4 kHz)
                if (t < 0.003) {
                    val += 0.35f * noiseDist(rng) * static_cast<float>(1.0 - t / 0.003);
                }
            }
            buffer.setSample(0, i, val);
            buffer.setSample(1, i, val);
        }

        calibrateToTargetRMS(buffer, targetRmsDbFS);
        return buffer;
    }

    /**
     * Generates a loud 808 Sub-Bass stem at -4.0 dBFS RMS (un-gain-staged).
     * 60 Hz tone with 180° inverted phase (destructive cancellation with Kick) and a +5 dB 250 Hz mud buildup.
     */
    static juce::AudioBuffer<float> generate808Stem(double sampleRate, double durationSeconds, float targetRmsDbFS = -4.0f) {
        const int numSamples = static_cast<int>(std::round(sampleRate * durationSeconds));
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        const int beatInterval = static_cast<int>(std::round(sampleRate * 0.5));
        double phaseMud250 = 0.0;

        for (int i = 0; i < numSamples; ++i) {
            const int posInBeat = i % beatInterval;
            const double t = static_cast<double>(posInBeat) / sampleRate;

            // 808 note envelope (sustained 350 ms decay)
            double subEnv = 0.25;
            if (posInBeat < static_cast<int>(sampleRate * 0.40)) {
                subEnv = 0.80 * std::exp(-t / 0.15);
            }

            // 60 Hz tone with 180° phase inversion (-sin)
            const double phase = 2.0 * std::numbers::pi * 60.0 * t;
            const float subVal = static_cast<float>(-std::sin(phase) * subEnv); // 180° inverted!

            // 250 Hz mud buildup (+5 dB stationary peak)
            phaseMud250 += 2.0 * std::numbers::pi * 250.0 / sampleRate;
            if (phaseMud250 >= 2.0 * std::numbers::pi) phaseMud250 -= 2.0 * std::numbers::pi;
            const float mudVal = 0.35f * static_cast<float>(std::sin(phaseMud250));

            const float total = subVal + mudVal;
            buffer.setSample(0, i, total);
            buffer.setSample(1, i, total);
        }

        calibrateToTargetRMS(buffer, targetRmsDbFS);
        return buffer;
    }

    /**
     * Generates a buried Lead Vocal stem at -24.0 dBFS RMS (un-gain-staged).
     * Vocal formants at 1-3 kHz with 7.5 kHz harsh sibilance bursts.
     */
    static juce::AudioBuffer<float> generateLeadVocalStem(double sampleRate, double durationSeconds, float targetRmsDbFS = -24.0f) {
        const int numSamples = static_cast<int>(std::round(sampleRate * durationSeconds));
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        std::mt19937 rng(999);
        std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

        double p800 = 0.0, p1500 = 0.0, p2800 = 0.0, p7500 = 0.0;
        const int phraseCycle = static_cast<int>(std::round(sampleRate * 2.0)); // 1.5s active, 0.5s pause
        const int activeDuration = static_cast<int>(std::round(sampleRate * 1.5));
        const int sibInterval = static_cast<int>(std::round(sampleRate * 0.6)); // Sibilance every 600ms

        for (int i = 0; i < numSamples; ++i) {
            const int posInPhrase = i % phraseCycle;
            float val = 0.0f;

            if (posInPhrase < activeDuration) {
                // Vowel formant harmonics
                p800 += 2.0 * std::numbers::pi * 880.0 / sampleRate;
                p1500 += 2.0 * std::numbers::pi * 1760.0 / sampleRate;
                p2800 += 2.0 * std::numbers::pi * 2640.0 / sampleRate;
                if (p800 >= 2.0 * std::numbers::pi) p800 -= 2.0 * std::numbers::pi;
                if (p1500 >= 2.0 * std::numbers::pi) p1500 -= 2.0 * std::numbers::pi;
                if (p2800 >= 2.0 * std::numbers::pi) p2800 -= 2.0 * std::numbers::pi;

                const float formant = 0.40f * static_cast<float>(std::sin(p800)) +
                                      0.35f * static_cast<float>(std::sin(p1500)) +
                                      0.25f * static_cast<float>(std::sin(p2800));

                // 7.5 kHz harsh sibilance burst ("s" / "t" sound)
                float sibilance = 0.0f;
                const int posInSib = i % sibInterval;
                if (posInSib < static_cast<int>(sampleRate * 0.060)) { // 60ms sibilance
                    p7500 += 2.0 * std::numbers::pi * 7500.0 / sampleRate;
                    if (p7500 >= 2.0 * std::numbers::pi) p7500 -= 2.0 * std::numbers::pi;
                    const double sibEnv = std::sin(std::numbers::pi * posInSib / (sampleRate * 0.060));
                    sibilance = 0.70f * static_cast<float>(sibEnv * (0.6 * std::sin(p7500) + 0.4 * noiseDist(rng)));
                }

                val = formant + sibilance;
            }

            buffer.setSample(0, i, val);
            buffer.setSample(1, i, val);
        }

        calibrateToTargetRMS(buffer, targetRmsDbFS);
        return buffer;
    }

    /**
     * Generates a loud stereo Synth Pad stem at -10.0 dBFS RMS (un-gain-staged).
     * Wide chorus pad wall centered at 1 kHz - 4 kHz masking the lead vocal,
     * accompanied by musical stereo air wash and cymbal shimmer.
     */
    static juce::AudioBuffer<float> generateSynthPadStem(double sampleRate, double durationSeconds, float targetRmsDbFS = -10.0f) {
        const int numSamples = static_cast<int>(std::round(sampleRate * durationSeconds));
        juce::AudioBuffer<float> buffer(2, numSamples);
        buffer.clear();

        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);
        std::array<float, 5> pinkStateL = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 5> pinkStateR = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        auto nextPink = [&](std::array<float, 5>& state) -> float {
            const float white = noiseDist(rng);
            state[0] = 0.99886f * state[0] + white * 0.0555179f;
            state[1] = 0.99332f * state[1] + white * 0.0750759f;
            state[2] = 0.96900f * state[2] + white * 0.1538520f;
            state[3] = 0.86650f * state[3] + white * 0.3104856f;
            state[4] = 0.55000f * state[4] + white * 0.5329522f;
            return (state[0] + state[1] + state[2] + state[3] + state[4] + white * 0.5362f) * 0.15f;
        };

        // Rich harmonic pad chord cluster centered at 1-4 kHz masking lead vocal
        const std::array<double, 6> freqs = {
            1174.66, 1396.91, 1760.00, 2093.00, 2637.02, 3135.96
        };
        const std::array<float, 6> weights = {
            0.20f, 0.20f, 0.18f, 0.18f, 0.14f, 0.12f
        };
        std::array<double, 6> phasesL {};
        std::array<double, 6> phasesR {};
        phasesL.fill(0.0);
        phasesR.fill(0.0);

        for (int i = 0; i < numSamples; ++i) {
            float outL = 0.0f;
            float outR = 0.0f;

            for (size_t k = 0; k < freqs.size(); ++k) {
                // Detuned left and right for wide stereo chorus
                const double incL = 2.0 * std::numbers::pi * (freqs[k] - 0.75) / sampleRate;
                const double incR = 2.0 * std::numbers::pi * (freqs[k] + 0.75) / sampleRate;

                phasesL[k] += incL;
                phasesR[k] += incR;
                if (phasesL[k] >= 2.0 * std::numbers::pi) phasesL[k] -= 2.0 * std::numbers::pi;
                if (phasesR[k] >= 2.0 * std::numbers::pi) phasesR[k] -= 2.0 * std::numbers::pi;

                outL += weights[k] * static_cast<float>(std::sin(phasesL[k]));
                outR += weights[k] * static_cast<float>(std::sin(phasesR[k]));
            }

            // Musical stereo air wash & cymbal shimmer (>2.5 kHz commercial pink profile)
            const float pinkL = 0.08f * nextPink(pinkStateL);
            const float pinkR = 0.08f * nextPink(pinkStateR);
            const float spreadL = 0.02f * noiseDist(rng);
            const float spreadR = -spreadL;

            outL += (pinkL + spreadL);
            outR += (pinkR + spreadR);

            buffer.setSample(0, i, outL);
            buffer.setSample(1, i, outR);
        }

        calibrateToTargetRMS(buffer, targetRmsDbFS);
        return buffer;
    }
};

} // namespace cli
