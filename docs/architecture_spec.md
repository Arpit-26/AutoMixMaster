# AutoMixMaster: System Architecture Specification
## Psychoacoustic Foundations, Differentiable Signal Processing, and VST3 Architecture for Automated Mixing and Mastering in FL Studio

---

## Executive Summary

"Professional" sound in modern music production is quantifiable: a finished song achieves genre-appropriate integrated loudness, controls True Peak at or below \(-1.0\text{ dBTP}\) (\(-2.0\text{ dBTP}\) for competitive hot masters), maintains a Peak-to-Short-Term Loudness Ratio (PSR) of at least \(8\text{ LU}\) in its loudest section, conforms to an empirical Long-Term Average Spectrum (LTAS) roll-off corridor (\(\sim -4.5\text{ dB/octave}\)), preserves sub-bass mono compatibility with under \(1.0\text{ dB}\) of mono-sum loss, and anchors the lead vocal approximately \(3\text{ LU}\) below the full mix short-term loudness.

**AutoMixMaster** is a hybrid "Glass-Box" Differentiable Digital Signal Processing (DDSP) and rule-based system engineered for FL Studio. Instead of utilizing opaque, artifact-prone black-box audio generation, AutoMixMaster extracts psychoacoustic features and modulates pristine, analytical C++20/JUCE DSP algorithms with full human transparency and parameter tweakability.

---

## 1. Acoustic and Psychoacoustic Foundations

### 1.1 The Convergence of Technical Precision and Aesthetic Context
- **Mixing vs. Mastering:** Mixing operates at the multitrack/subgroup level to balance levels, sculpt frequency space, control dynamics, place elements in the stereo/depth panorama ("stage metaphor"), and blend spaces via reverb sends. Mastering acts as the final quality-assurance checkpoint: optimizing global LTAS, regulating micro- and macro-dynamics, eliminating harsh resonances, and ensuring compliance with distribution ceilings.
- **Dynamic Context Adaptation:** Psychoacoustic research indicates expert mix decisions dynamically adapt to monitoring environments: under high-fidelity conditions, engineers prioritize global spectral balance; under degraded acoustic conditions, they prioritize track separation and vocal comprehension.
- **Genre Contour Variation:** Spectral roll-off differences between genres are primarily a physical side-effect of percussive prominence and arrangement density, not arbitrary tonal preferences. Static EQ curves fail; target spectral envelopes must scale with transient and percussive density.

### 1.2 Telemetry of Independent Mixes vs. Commercial Standards
Analysis of 3,082 pre-mastered independent mixes reveals severe bimodal distribution:
- **Loudness Spread:**
  - Median: \(-14.0\text{ LUFS}\) (Interquartile Range: \(-16.0\) to \(-11.0\text{ LUFS}\)).
  - \(22.5\%\) arrive quieter than \(-16.0\text{ LUFS}\); \(10.0\%\) arrive at \(-21.0\text{ LUFS}\) or lower (raw, uncompressed tracks).
  - \(49.6\%\) arrive louder than \(-14.0\text{ LUFS}\); \(16.4\%\) arrive at \(-9.0\text{ LUFS}\) or hotter (pre-clipped/pre-limited).
  - \(47.6\%\) are submitted as lossy MP3/M4A rather than uncompressed PCM.
- **Dynamic Headroom (PLR):**
  - Median incoming Peak-to-Loudness Ratio (PLR): \(13.0\text{ dB}\) (IQR: \(11.0\) to \(15.0\text{ dB}\)).
  - \(38.8\%\) have open headroom (\(\ge 14.0\text{ dB}\)).
  - \(12.3\%\) arrive at \(\le 9.0\text{ dB}\); \(6.6\%\) are crushed at \(\le 8.0\text{ dB}\).
- **The "Over-Limiting Death Spiral":** When a mix arrives heavily pre-limited (e.g., from default Fruity Limiter templates), downstream limiters destroy transient snap, turning kicks and snares into distorted square waves. The plugin must detect pre-limited inputs and route excess dynamic management upstream to soft clippers and multiband crest control.

### 1.3 Streaming Normalization Mechanics (2026 Standards)
Over \(87\%\) of consumers stream via loudness-normalized platforms. Mastering louder than platform targets yields zero volume benefit while causing permanent transient damage.

| Platform / Medium | Integrated Target | True Peak Ceiling | Normalization Behavior | Delivery Codec |
| :--- | :--- | :--- | :--- | :--- |
| **Spotify** | \(-14\text{ LUFS}\) (Loud: \(-11\), Quiet: \(-19\)) | \(-1.0\text{ dBTP}\) (\(-2.0\text{ dBTP}\) if \(> -14\text{ LUFS}\)) | Up & Down (Peak-constrained boost) | Ogg Vorbis / AAC |
| **Apple Music** | \(-16\text{ LUFS}\) (Sound Check) | \(-1.0\text{ dBTP}\) | Down Only (Album / Track aware) | AAC 256 kbps / ALAC |
| **YouTube** | \(-14\text{ LUFS}\) (\(-13\) to \(-15\)) | \(-1.0\text{ dBTP}\) | Down Only (Never boosts quiet tracks) | Opus / AAC |
| **Tidal** | \(-14\text{ LUFS}\) | \(-1.0\text{ dBTP}\) | Up & Down / Down Only (Album aware) | FLAC / AAC |
| **Amazon Music** | \(-14\text{ LUFS}\) | \(-1.0\text{ dBTP}\) | Down Only (User toggleable) | FLAC / AAC |
| **Deezer** | \(-15\text{ LUFS}\) | \(-1.0\text{ dBTP}\) | Up & Down | FLAC / MP3 320 kbps |
| **SoundCloud** | \(-14\text{ LUFS}\) (or unnormalized) | \(-1.0\text{ dBTP}\) (\(-2.0\text{ dBTP}\) if hot) | Platform dependent | Opus / MP3 / AAC |
| **TikTok / Reels** | \(-14\text{ LUFS}\) | \(-1.0\text{ dBTP}\) | Aggressive re-encoding + DSP | Low-bitrate AAC / Ogg |
| **Club / Beatport** | None (\(-8\) to \(-6\text{ LUFS}\) typical) | \(-1.0\text{ dBTP}\) | None (Direct unnormalized playback) | WAV / FLAC / MP3 |
| **CD / Master PCM** | None (\(-9\) to \(-7\text{ LUFS}\) typical) | \(-0.3\text{ dBTP}\) | None | 16-bit/44.1k or 24-bit/48k |

### 1.4 Genre-Specific Targets & Headroom Corridors

| Musical Genre / Profile | Recommended LUFS Window | Target LRA | True Peak Ceiling & Dynamic Strategy |
| :--- | :--- | :--- | :--- |
| **EDM / Club / Trap** | \(-8.0\) to \(-6.0\text{ LUFS}\) | \(3.0\) to \(5.0\text{ LU}\) | \(-2.0\text{ dBTP}\); dense soft-clipping on drum transients to protect sub headroom |
| **Hip-Hop / Urban** | \(-9.0\) to \(-7.0\text{ LUFS}\) | \(4.0\) to \(8.0\text{ LU}\) | \(-1.0\) to \(-2.0\text{ dBTP}\); clean sub/808 transient punch |
| **Pop / Modern Rock** | \(-10.0\) to \(-8.0\text{ LUFS}\) | \(5.0\) to \(10.0\text{ LU}\) | \(-1.0\text{ dBTP}\); punchy drums, forward vocal articulation |
| **Indie / Singer-Songwriter** | \(-14.0\) to \(-11.0\text{ LUFS}\) | \(6.0\) to \(12.0\text{ LU}\) | \(-1.0\text{ dBTP}\); natural acoustic dynamics, transparent bus glue |
| **Jazz / Acoustic / Classical** | \(-18.0\) to \(-14.0\text{ LUFS}\) | \(10.0\) to \(20.0+\text{ LU}\) | \(-1.0\text{ dBTP}\); unclipped crest factor, minimal transparent limiting |

### 1.5 Master Presets & Dynamic Guards
1. **Streaming-Safe (Default):** Integrated \(-14.0\text{ LUFS} \pm 0.5\text{ LU}\), True Peak \(\le -1.0\text{ dBTP}\), Minimum \(\text{PSR} \ge 10.0\text{ LU}\).
2. **Competitive (Pop/Club/Hip-Hop):** Integrated \(-9.0\) to \(-11.0\text{ LUFS}\), True Peak \(\le -2.0\text{ dBTP}\), Minimum \(\text{PSR} \ge 8.0\text{ LU}\) in loudest 30-second section.
3. **Dynamic (Acoustic/Classical/Jazz):** Integrated \(-14.0\) to \(-18.0\text{ LUFS}\), True Peak \(\le -1.0\text{ dBTP}\), Global \(\text{PLR} \ge 12.0\text{ dB}\), \(\text{LRA} \ge 10.0\text{ LU}\).

### 1.6 Tonal Balance Corridors & Spectral Pathology
Commercial releases converge on an average spectral roll-off of \(-4.5\text{ dB/octave}\) (between pink noise \(-3\text{ dB/oct}\) and brown noise \(-6\text{ dB/oct}\)). Target curves group into three primary families: **Bass Heavy**, **Modern**, and **Orchestral**.

| Frequency Band | Spectral Region | Primary Musical Elements | Symptom if Excessive | Symptom if Deficient |
| :--- | :--- | :--- | :--- | :--- |
| **20–60 Hz** | Sub-Bass | Kick & 808 fundamental | Eats headroom, pumps limiter, muddy on small speakers | Lacks weight and visceral physical impact |
| **60–250 Hz** | Bass | Kick body, bass guitar fundamentals | Boomy, masks low midrange instruments | Thin, weak, lacking foundational support |
| **250–500 Hz** | Low-Mids | Vocal chest, snare body, guitar warmth | Boxy, cloudy, claustrophobic buildup | Hollow, cold, lacking acoustic body |
| **500 Hz–2 kHz** | Mids | Core instrument harmonics | Honky (1 kHz) or tinny (1–2 kHz) | Scooped, disconnected, weak ensemble |
| **2–6 kHz** | Upper-Mids | Vocal intelligibility, transient bite | Piercing, harsh, rapid listener fatigue | Dull, distant, lacks presence and articulation |
| **6–10 kHz** | Brilliance | Sibilance, hi-hat & cymbal sizzle | Sharp, sibilant, fatiguing | Lifeless, muted transient air |
| **10–20 kHz** | Air | Breath, shimmer, ultra-high reverb tails | Brittle, noisy, exaggerated hiss | Closed-in, narrow three-dimensional space |

---

## 2. Core Signal Processing Formulations

### 2.1 Chain Complexity & Generality
Empirical studies of professional mixing interventions (SAFE Project: 178 mixes, 47 engineers) demonstrate:
- \(50.6\%\) of mixing tasks require only 1 processor; \(36.0\%\) require 2 processors.
- \(86.6\%\) of all tasks are accomplished with \(\le 2\) processors.
- **Parametric EQ** (\(69.7\%\)) and **Compressors** (\(40.4\%\)) form the universal backbone. Reverb (\(32.0\%\)) and Distortion (\(22.5\%\)) are strictly context-dependent.
- Canonical per-track topology:
  $$\text{Input Gain} \longrightarrow \text{Corrective Subtractive EQ} \longrightarrow \text{Dynamics} \longrightarrow \text{Additive Tone} \longrightarrow \text{Parallel Reverb/Delay Send}$$

### 2.2 Sample-Rate Adaptive ITU-R BS.1770-4 K-Weighting
Hardcoding 48 kHz biquad coefficients breaks accuracy in 44.1 kHz, 88.2 kHz, 96 kHz, or 192 kHz sessions. Coefficients must be derived via bilinear transform:

1. **Stage 1 (Head-Diffraction High-Shelf Filter):**
   - \(f_0 = 1681.974450955533\text{ Hz}\), \(G = +3.999843853973347\text{ dB}\), \(Q = 0.7071752369554196\).
   - Compute:
     $$K = \tan\left(\frac{\pi f_0}{f_s}\right), \quad V_h = 10^{G/20}, \quad V_b = V_h^{0.4996667741545416}, \quad a_0 = 1.0 + \frac{K}{Q} + K^2$$
     $$b_0 = \frac{V_h + V_b \frac{K}{Q} + K^2}{a_0}, \quad b_1 = \frac{2(K^2 - V_h)}{a_0}, \quad b_2 = \frac{V_h - V_b \frac{K}{Q} + K^2}{a_0}$$
     $$a_1 = \frac{2(K^2 - 1.0)}{a_0}, \quad a_2 = \frac{1.0 - \frac{K}{Q} + K^2}{a_0}$$

2. **Stage 2 (RLB High-Pass Filter):**
   - \(f_0 = 38.13547087602444\text{ Hz}\), \(Q = 0.5003270373238773\).
   - Compute:
     $$K = \tan\left(\frac{\pi f_0}{f_s}\right), \quad a_0 = 1.0 + \frac{K}{Q} + K^2$$
     $$b_0 = \frac{1.0}{a_0}, \quad b_1 = \frac{-2.0}{a_0}, \quad b_2 = \frac{1.0}{a_0}, \quad a_1 = \frac{2(K^2 - 1.0)}{a_0}, \quad a_2 = \frac{1.0 - \frac{K}{Q} + K^2}{a_0}$$

3. **Gating Algorithm:**
   - 400 ms rectangular blocks, 75% overlap (100 ms step).
   - Block loudness:
     $$L_j = -0.691 + 10 \log_{10}\left( \sum_{i} G_i \cdot z_{i,j} \right)$$
   - Absolute gate at \(-70.0\text{ LUFS}\).
   - Relative gate: calculate mean energy of blocks surviving absolute gate, then discard blocks \(< (\text{Mean} - 10.0\text{ LU})\).

### 2.3 Empirical Balance Anchors & Rules
- **-18 dBFS RMS Input Calibration:** All track inputs normalized to \(-18\text{ dBFS RMS}\) (peaks \(-12\) to \(-6\text{ dBFS}\), corresponding to analog 0 VU).
- **-3 LU Lead Vocal Anchor:** Lead vocal balanced at \(-3.0\text{ LU} \pm 1.5\text{ LU}\) relative to full mix short-term loudness across all sections.
- **Universal High-Pass Filtering:** Low frequencies stripped on non-bass tracks (vocals: 80–120 Hz; snare: 80–100 Hz; guitars/keys: 100–150 Hz; hats: 300–500 Hz).
- **Transient-Passing Compression:** Attack times set slow enough (3–15 ms) to let transient spikes through, increasing crest factor.
- **-9 LU Reverb Send Anchor:** Auxiliary sends set \(\approx -9.0\text{ LU}\) below dry source, high-passed at 200–300 Hz with 10–30 ms pre-delay.
- **Tempo Calculations:**
  $$t_{1/4} = \frac{60000}{\text{BPM}}\text{ ms}, \quad t_{1/8} = 0.5 \cdot t_{1/4}, \quad t_{\text{dotted } 1/8} = 0.75 \cdot t_{1/4}$$

### 2.4 Source-Specific Processing Matrix

| Track Class | Clean-Up EQ | Dynamics / Compression | Tonal Shaper & Reverb | Spatial & Routing |
| :--- | :--- | :--- | :--- | :--- |
| **Lead Vocal** | HPF 80–120 Hz; cut 200–400 Hz mud; notch harshness | 2:1–4:1, medium attack/release, 3–6 dB GR; post-comp de-esser 5–9 kHz | +1–2 dB shelf @ 3–5 kHz & 12 kHz air; subtle saturation; reverb @ -9 LU (15–30 ms pre-delay) | Center; auto-fader ride to maintain -3 LU anchor |
| **Backing Vocals** | Steeper HPF than lead; dip 2–4 kHz to clear room | Heavier compression (tight dynamic envelope) | Darker top end; higher reverb send level | Panned wide L/R; tucked below lead |
| **Kick Drum** | Strip subsonic rumble; notch 100–150 Hz or 230 Hz boxiness | 4:1–8:1, 5–15 ms attack (preserves transient), 50–200 ms release | Weight boost @ 50–80 Hz; click boost @ 3–5 kHz | Dead center; strictly mono |
| **Snare Drum** | HPF below fundamental; cut 500–700 Hz boxiness | 4:1–8:1, 3–15 ms attack (preserves crack), 50–250 ms release | Body weight @ 150–250 Hz; snap @ 5 kHz; short plate reverb send | Center |
| **808 / Sub-Bass** | HPF below 25–30 Hz; cut 220–500 Hz mud | 3:1–6:1 leveling; 1–3 dB fast sidechain duck from kick (60–150 Hz) | Tube/tape saturation for upper harmonics on mobile speakers | Center; mono below 120 Hz |
| **Bass Guitar** | HPF below 35 Hz; cut 230–500 Hz mud | 3:1–6:1 (Opto/FET), medium-fast attack, moderate release | Weight @ 60–100 Hz; pick articulation @ 700 Hz–1 kHz | Center; mono below 120 Hz |
| **Cymbals / Hats** | Steep HPF (300–500 Hz); dynamic resonance notch on harsh ring | Light peak control on rapid rolls | Shimmer @ 7.5–12 kHz; tame 3–6 kHz piercing frequencies | Wide stereo; offset from center |
| **Pads & Guitars** | HPF 100–150 Hz; cut 200–400 Hz; dynamic EQ dip @ 1–4 kHz from vocal | Light 2:1–3:1 smoothing compression | Stereo widening; chorus/delay modulation | Wide stereo framing vocal |
| **Mix Bus (Pre-Master)** | 20 Hz subsonic HPF; broad shelves capped \(\pm 3\text{ dB}\) | 1.5:1–2:1 VCA glue, slow attack, auto release, 1–3 dB GR | Mid/High tape exciter; M/S stereo width | Mono below 120 Hz; peaks \(-6\text{ dBFS}\) |

### 2.5 Cross-Adaptive Masking, Phase & Stereo Space
- **XA-DAFx Spectral Ducking:** Multi-band sidechain ducking in the 60–150 Hz region on bass when the kick hits (2–6 dB reduction, 4:1 ratio, 1 ms attack, 60–120 ms release) leaves upper harmonics untouched.
- **Automated Phase Rotation:** Continuous \(0^{\circ}\) to \(360^{\circ}\) phase sweep analysis between layered low-end tracks (e.g. acoustic kick + 808) to lock polarity and phase angle that maximizes summed energy.
- **Sub-Bass Mono Enforcement:** Measure mono-sum loss:
  $$\text{Loss} = \text{LUFS}_{\text{stereo}} - \text{LUFS}_{(L+R)/2}$$
  If loss in the \(<120\text{ Hz}\) band exceeds \(1.0\text{ dB}\) or band correlation drops below \(+0.3\), collapse width to 0% below 120 Hz.
- **Electrostatic Repulsion Panning:** Midrange instruments treated as charged particles repelled according to fundamental frequency differences:
  $$F = k \cdot \frac{q_u q_v}{r^2}, \quad q_u q_v = \frac{1}{|f_{0,u} - f_{0,v}|}$$
  Hard semantic constraints keep kick, snare, bass, and lead vocals anchored to center.

### 2.6 Dynamic Resonance Suppression vs. Dynamic EQ

| Feature | Static / Dynamic EQ | Dynamic Resonance Suppressor | Adaptive Spectral Compressor |
| :--- | :--- | :--- | :--- |
| **Mechanism** | 1–24 manual biquad / linear-phase bands | Continuous full-spectrum FFT peak detection generating dynamic notches | Continuous multi-band upward/downward leveling across spectrum |
| **Target Material** | Stationary resonances, fixed kick-bass ducking | Pitch-tracking vocal harshness, cymbal fizz, whistle | Dense full mixes with shifting tonal balance |
| **Threshold** | Absolute dBFS threshold | Adaptive level-independent soft threshold | Hysteresis / variance-driven spectral threshold |
| **Latency/Phase** | Zero-latency IIR or linear-phase | Low-latency tracking or linear-phase mastering | Transparent low-latency |
| **Risk** | Hollow tone if over-notched | Strips harmonic bite if too deep | Destabilizes stereo width if aggressive |

### 2.7 Ballistic Compressor & Dual-Envelope Transient Shaper
- **Giannoulis Decoupled Level Detector:**
  $$y_L[n] = \begin{cases} \alpha_A y_L[n-1] + (1-\alpha_A)|x[n]| & \text{if } |x[n]| > y_L[n-1] \\ \alpha_R y_L[n-1] + (1-\alpha_R)|x[n]| & \text{if } |x[n]| \le y_L[n-1] \end{cases}$$
  where \(\alpha = \exp(-1 / (\tau f_s))\).
- **Soft-Knee Gain Computer:**
  $$y_G[n] = \begin{cases} x_G[n] & \text{for } 2(x_G - T) < -W \\ x_G[n] + \frac{(1/R - 1)(x_G - T + W/2)^2}{2W} & \text{for } 2|x_G - T| \le W \\ T + \frac{x_G - T}{R} & \text{for } 2(x_G - T) > W \end{cases}$$
- **Level-Independent Dual-Envelope Transient Shaper:**
  $$\Delta E[n] = \frac{E_{\text{fast}}[n] - E_{\text{slow}}[n]}{E_{\text{slow}}[n] + \epsilon}$$
  where \(E_{\text{fast}} \approx 2\text{ to }5\text{ ms}\) and \(E_{\text{slow}} \approx 20\text{ to }50\text{ ms}\). \(\Delta E > 0\) isolates onset attack; \(\Delta E < 0\) isolates decay.

---

## 3. Intelligent Music Production & System Architecture

### 3.1 Differentiable Mixing Console ("Glass-Box" DDSP)
- **Zero Black-Box Audio Synthesis:** Audio is never generated or processed through neural hidden layers. Audio passes through pure, deterministic C++ DSP blocks.
- **Interpretable Parameter Prediction:** Lightweight neural models (or rule-based optimizers) predict explicit, human-readable parameters: EQ frequencies, gains, Q factors, compressor thresholds, attack/release times.
- **Reference Style Transfer:** Extract LTAS, dynamic contour, and stereo width profile from user-supplied reference tracks and map them onto the DSP parameters.

### 3.2 Dual-Mode System Topology
1. **Track/Bus Insert Mode (Mix Tap):**
   - Placed on individual mixer inserts (Vocal, Kick, Snare, Bass, Bus).
   - Normalizes input to \(-18\text{ dBFS RMS}\).
   - Classifies track type (spectral centroid, crest factor, low-energy ratio).
   - Applies class-specific chain and shares telemetry with Master via lock-free shared memory.
2. **Master Bus Mode (Mastering Console):**
   - Placed on the FL Studio Master insert.
   - Evaluates mix bus PLR, PSR, LTAS, mono-sum loss.
   - Executes 10-stage mastering chain with closed-loop verification.

### 3.3 Two-Tier Hybrid Inference Architecture
- **Tier 1 (Real-Time Audio Thread):**
  - Header-only `RTNeural` engine using compile-time `RTNeural::ModelT` templates.
  - Zero heap allocations (`malloc`, `new`, `std::vector::resize`) inside `processBlock()`.
  - Sub-millisecond inference for analog saturation modeling and fast threshold adaptation.
- **Tier 2 (Asynchronous Worker Thread):**
  - Lock-free SPSC FIFO (`juce::AbstractFifo`) transfers audio frames from audio thread.
  - Computes 30-second feature analysis: LUFS, PLR, crest factor, MFCCs, spectral roll-off, chroma key, fundamental pitch.
  - Predicts target parameter vectors and pushes them back to audio thread via atomic parameter listeners smoothed over 20–50 ms.

### 3.4 FL Studio Host Quirks & Solutions
1. **Variable-Sized Audio Buffers:** FL Studio splits callbacks into arbitrary sizes (e.g. 17, 64, 513 samples) during automation. Fixed-size FFT algorithms must decouple via internal circular ring buffers (`internalFifoBuffer`), and latency must be reported via `AudioProcessor::setLatencySamples()`.
2. **Sidechain Bus Declaration:** Must declare main stereo pair + aux stereo sidechain input pair in the constructor and support them in `isBusesLayoutSupported()`.
3. **FL Studio Native Mixer Control:**
   - Use FL Studio's Python MIDI-Controller Scripting Bridge (`setTrackVolume`, `setTrackPan`, `getTrackName`) via a local socket or loopback MIDI to automate native mixer faders.

---

## 4. Deterministic 10-Stage Mastering Pipeline

1. **Input Trim & Subsonic Filter:** Normalize mix peaks to \(-6.0\text{ dBFS}\) (\(-18\text{ dBFS RMS}\) for stems); 20 Hz high-pass filter (18 dB/oct) removes DC and sub-audible mud.
2. **Corrective Linear-Phase EQ:** Surgical narrow cuts (capped at \(3.0\text{ dB}\)) eliminate stationary resonant frequencies.
3. **Mix-Bus Glue Compressor:** 1.5:1 to 2:1 VCA compressor, slow attack (30 ms), auto release, K-weighted sidechain filter, \(1.0\) to \(2.0\text{ dB}\) transparent GR.
4. **Low-Band Crest & Dynamic EQ:** Multiband crest control below 250 Hz stabilizes kick/sub punch; dynamic EQ tames 2–5 kHz harshness.
5. **Mid/Side Crossover & Stereo Width:** Collapses Side below 120 Hz if sub mono loss \(> 1.0\text{ dB}\); applies subtle high-shelf Side boost if narrow, rolled back if correlation \(< 0.0\).
6. **Harmonic Saturation:** Subtle even-order (tube) or odd-order (tape) saturation for density, evaluated at matched loudness.
7. **Tonal-Balance Target EQ:** Broad shelves and wide bells (capped at \(\pm 3.0\text{ dB}\) on master, \(\pm 6.0\text{ dB}\) on stems) guide LTAS into target genre corridor.
8. **Dynamic Resonance Suppressor:** Level-independent soft-threshold suppression from 200 Hz to 12 kHz, max notch depth \(6.0\) to \(9.0\text{ dB}\).
9. **Oversampled Soft Clipper:** Shaves \(1.0\) to \(2.0\text{ dB}\) off transient peaks using 4x–16x oversampling or ADAA to prevent brickwall limiter overload.
10. **True-Peak Limiter & Dither:** 4x polyphase FIR True-Peak Limiter (128-tap Kaiser window, \(\beta = 8.5\)) drives level to target LUFS (\(\pm 0.5\text{ LU}\)). Final 24-bit/session-rate export with TPDF dither.

---

## 5. 12-Point Automated Render Verification Matrix

Every render must satisfy the following 12 verification checkpoints. If any check fails during headless or background verification, the state machine triggers the corresponding first-pass DSP fallback:

| # | Check Name | Target Specification | Failure Condition | Automated First-Pass DSP Fallback |
| :-: | :--- | :--- | :--- | :--- |
| **1** | **Input Pre-Limit Guard** | Peaks \(\approx -6\text{ dBFS}\); \(\text{PLR} \ge 11.0\text{ dB}\) | Incoming \(\text{PLR} \le 8.0\text{ dB}\) and \(\text{LUFS} \ge -11.0\) | Trim input to \(-18\text{ dBFS RMS}\); bypass aggressive maximizer stages to prevent over-limiting |
| **2** | **Integrated Loudness** | Preset target \(\pm 0.5\text{ LU}\) (\(-14\) Safe; \(-9\) to \(-11\) Competitive) | Deviation \(> \pm 1.0\text{ LU}\) from target | Adjust True-Peak Limiter threshold/drive in closed loop |
| **3** | **True Peak Ceiling** | \(\le -1.0\text{ dBTP}\) (\(\le -2.0\text{ dBTP}\) if master \(> -14\text{ LUFS}\)) | 4x oversampled peak exceeds ceiling | Switch limiter to 128-tap Kaiser True-Peak mode and clamp ceiling |
| **4** | **Minimum PSR** | \(\text{PSR} \ge 8.0\text{ LU}\) (\(\ge 10.0\text{ LU}\) for Streaming-Safe) in loudest 30s | \(\text{PSR} < 8.0\text{ LU}\) in chorus or drop | Back off limiter drive; shift 1–2 dB peak reduction to oversampled soft clipper |
| **5** | **Limiter Gain Reduction** | \(2.0\) to \(4.0\text{ dB}\) peak attenuation | Sustained \(\text{GR} > 6.0\text{ dB}\) | Shift 1–2 dB to bus glue compressor / low-band crest control; report mix density deficit |
| **6** | **4-Band Tonal Balance** | Inside target corridor (<250 Hz, 250–2k, 2k–8k, >8k; \(\approx -4.5\text{ dB/oct}\)) | Any band exits family/reference corridor during main sections | Apply broad M/S shelves or wide bells (capped \(\pm 3\text{ dB}\) Master, \(\pm 6\text{ dB}\) stems) |
| **7** | **Low-Band Crest Factor** | Within target dynamic envelope below 250 Hz | Crest factor too high (pumping limiter) or too low (crushed) | Adjust low-band (<250 Hz) multiband compressor threshold/ratio |
| **8** | **Dynamic Resonance Control** | Smooth 2–6 kHz presence without harsh spikes | Moving resonances or sibilance exceed local spectrum | Engage post-compression spectral de-resonator with 6–9 dB maximum cut ceiling |
| **9** | **Multi-Band Correlation** | \(+0.3\) to \(+0.9\) overall; \(> 0.0\) in all 4 bands | Sustained correlation \(< 0.0\) in any frequency band | Roll back M/S Side boost or widener in band; sweep \(0^{\circ}\) to \(360^{\circ}\) phase rotation |
| **10** | **Sub-Band Mono-Sum Loss** | \((L+R)/2\) loudness loss \(< 1.0\text{ dB}\) below 120 Hz | Sub-band mono sum loses \(> 1.0\text{ dB}\) | Collapse Side channel to 0% width below crossover (120 Hz default; 90–250 Hz club PA) |
| **11** | **Lead Vocal Relative Level** | \(-3.0\text{ LU} \pm 1.5\text{ LU}\) relative to full mix short-term LUFS | Vocal falls outside \(-5.0\) to \(-1.0\text{ LU}\) window | Write section-aware fader automation via Python bridge + apply 2–3 dB dynamic EQ dip on music bus |
| **12** | **Reverb Return & Export** | Reverb \(\approx -9.0\text{ LU}\) below dry source; 24-bit WAV export | Reverb tail masks vocal; bit depth reduced before SRC or double-dithered | Trim reverb return to \(-9\text{ LU}\) and HPF at 250 Hz; enforce SRC before single final TPDF dither |

---

## 6. Implementation Roadmap

- **Phase 1 (Master-Only MVP):** Single Master-insert VST3 plugin housing full ITU-R BS.1770-4 metering, 4-band tonal balance analyzer, multi-band phase correlation, 10-stage mastering chain, closed-loop loudness loop, 3 master presets, and Matchering-style reference matching.
- **Phase 2 (Per-Track Taps & Source Chains):** Lightweight per-track "Mix Node / Tap" instances communicating via shared memory. Automated -18 dBFS RMS gain staging, hybrid instrument classification, source-specific chains, and kick-to-bass dynamic EQ ducking.
- **Phase 3 (Native Balance & Section Automation):** Companion FL Studio Python MIDI-Controller script bridge to automate native mixer faders, enforce the -3 LU lead vocal anchor across sections, and calibrate reverb returns.
- **Phase 4 (Learned Differentiable Parameter Prediction):** Train lightweight transformer/MLP models on user mix adjustments, deploying via RTNeural to predict style-transfer parameters directly onto the DSP console.
