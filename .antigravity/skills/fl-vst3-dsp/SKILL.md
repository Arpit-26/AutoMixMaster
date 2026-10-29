---
name: fl-vst3-dsp
description: Real-time C++20/JUCE 8 DSP engineering rules, FL Studio VST3 wrapper safeguards, and ITU-R BS.1770-4 verification protocols for the Automated Mix & Master plugin.
---

# FL Studio VST3 & Real-Time DSP Engineering Rules

## 1. Non-Negotiable Real-Time Audio Thread Constraints
- **Zero Heap Allocations in `processBlock()`:** Never call `new`, `delete`, `malloc`, `free`, `std::vector::push_back`, `std::vector::resize`, or `std::string` allocations inside `AudioProcessor::processBlock()`.
- **Pre-Allocation in `prepareToPlay()`:** Pre-allocate all audio buffers, FFT workspaces, polyphase FIR delay lines, and `RTNeural::ModelT` compile-time templates inside `prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)`.
- **Lock-Free Thread Communication:** Never use `std::mutex`, `std::lock_guard`, or OS system calls on the audio thread. Use `std::atomic<float>` for scalar parameters and a lock-free Single-Producer Single-Consumer (SPSC) ring buffer (`juce::AbstractFifo`) to pass audio frames to the background optimization worker thread.

## 2. FL Studio Host & Wrapper Compatibility
- **Variable Buffer Decoupling:** FL Studio slices `processBlock()` callbacks into variable sample lengths (from 1 sample up to the audio interface buffer size) during automation. Any fixed-window STFT/FFT processor or lookahead stage MUST route through an internal circular FIFO buffer and report exact latency via `setLatencySamples()`.
- **VST3 Sidechain Bus Layout:** Always declare a main stereo input/output bus (enabled by default) and an auxiliary stereo sidechain input bus (disabled by default) in the `AudioProcessor` constructor, and validate both configurations inside `isBusesLayoutSupported()` for FL Studio's `Wrapper Settings -> Processing` tab.

## 3. Mathematical DSP Specifications
- **Sample-Rate Adaptive ITU-R BS.1770-4 K-Weighting:** Never hardcode the 48 kHz biquad coefficients from Table 1 of ITU-R BS.1770-4. Always compute the bilinear transform coefficients analytically from the active host `sampleRate` (44.1 kHz to 192 kHz) for Stage 1 (high-shelf: f0 = 1681.9745 Hz, G = +3.9998 dB, Q = 0.7072) and Stage 2 (RLB high-pass: f0 = 38.1355 Hz, Q = 0.5003).
- **True-Peak Oversampling:** Compute True Peak (dBTP) using a 4x polyphase FIR interpolation filter (48-tap ITU reference mode for live monitoring; 128-tap Kaiser window with beta = 8.5 for mastering renders).
- **Gain Staging & Mix Anchors:** Normalize individual stems to -18.0 dBFS RMS prior to dynamics processing. Anchor lead vocals to -3.0 LU (+/- 1.5 LU) relative to the full mix short-term loudness, and auxiliary reverb returns to -9.0 LU below the dry source.

## 4. Mandatory Headless Verification Protocol
Because agents cannot listen to audio directly, every DSP change must be verified via terminal execution before marking a task complete:
1. Build both `AutoMixMaster_VST3` and `AutoMixMaster_CLI` using CMake.
2. Run `AutoMixMaster_CLI <input.wav> <output.wav> --preset competitive` and inspect the generated `verification_report.md` artifact.
3. Verify all guards pass:
   - Integrated Loudness within +/- 0.5 LU of preset target.
   - 4x True Peak <= -1.0 dBTP (<= -2.0 dBTP when louder than -14.0 LUFS).
   - Minimum PSR (True Peak minus 3s Short-Term Loudness in loudest 30s) >= 8.0 LU.
   - Limiter Gain Reduction sustained <= 6.0 dB (2.0 to 4.0 dB nominal).
   - Sub-band (<120 Hz) mono-sum loss < 1.0 dB and overall correlation in [+0.3, +0.9].
4. Cross-check LUFS and True Peak accuracy against `ffmpeg -i <output.wav> -af ebur128=peak=true -f null -` within +/- 0.1 LU.
