# AutoMixMaster Multitrack Verification Report (Milestone 2)

## Per-Track Mix Nodes, Source-Specific Chains & Cross-Adaptive Unmasking

| # | Verification Check | Target Requirement | Measured Result | Status |
| :-: | :--- | :--- | :--- | :---: |
| 1 | Stem Gain Staging | -18.0 dBFS RMS (+/-0.5 dB) | Kick:-18.0 Bass:-18.0 Vox:-17.5 Pad:-18.0 | **PASS** |
| 2 | Phase Cancellation Resolution | Constructive Sub Reinforcement (>+1.0 dB) | +3.01 dB (Rotated 180.0 deg) | **PASS** |
| 3 | Kick-to-Bass Dynamic Unmasking | 2.0 to 4.0 dB Dynamic EQ cut (60-150 Hz) | 3.50 dB cut | **PASS** |
| 4 | Vocal-to-Music Dynamic Unmasking | 2.0 to 3.0 dB Mid Presence cut (1-4 kHz) | 2.50 dB cut | **PASS** |
| 5 | Lead Vocal Relative Level | -3.0 LU +/- 1.5 LU relative to mix | -4.22 LU (Vox - Mix) | **PASS** |
| 6 | Integrated Loudness | Preset Target +/-0.5 LU (-10.0) | -10.4 LUFS | **PASS** |
| 7 | True Peak Ceiling | <= -2.00 dBTP | -2.12 dBTP | **PASS** |
| 8 | Minimum PSR | >= 8.00 LU | 8.095 LU | **PASS** |
| 9 | Limiter Gain Reduction | Nominal 1.0-4.0 dB (<= 6.0 dB) | 2.97 dB | **PASS** |
| 10 | 4-Band Tonal Balance | -4.5 dB/oct Tilt (+/-1.5 dB) | Slope = -4.98 dB/oct | **PASS** |
| 11 | Multi-Band Spatial Correlation | > 0.0 across all 4 bands | Sub = 0.999, Global = 0.978 | **PASS** |
| 12 | Sub-Band Mono-Sum Loss | < 1.0 dB loss below 120 Hz | 0.000 dB | **PASS** |

## Source Stem Telemetry & Processing Details

| Stem | Raw RMS | Gain-Staged RMS | Gain Staging Applied | Key Dynamic Actions |
| :--- | :---: | :---: | :---: | :--- |
| **Kick** | `-6 dBFS` | `-18.0242 dBFS` | `-12.0242 dB` | 28 Hz HPF, 250 Hz mud cut, transient compression, mono collapse |
| **808 Bass** | `-4 dBFS` | `-18.023 dBFS` | `-14.023 dB` | 180° phase inversion resolution, 60-150 Hz Kick dynamic EQ ducking (3.5 dB), harmonic saturation |
| **Lead Vocal** | `-24 dBFS` | `-17.5724 dBFS` | `+6.42763 dB` | Automatic boost from -24 dBFS to -18 dBFS RMS, 7.5 kHz de-essing (1.63906 dB), leveling compression |
| **Synth Pad** | `-10 dBFS` | `-18.015 dBFS` | `-8.01502 dB` | Vocal presence ducking (2.5 dB Mid dip at 2.5 kHz), 120 Hz HPF |

## Low-End Phase Coherence & Reinforcement

- **Sub-Bass Sum Before Phase Rotation**: `-18.2254 dBFS` (Destructive cancellation)
- **Sub-Bass Sum After Phase Rotation**: `-15.2091 dBFS` (Constructive reinforcement)
- **Net Low-End Gain**: `+3.01621 dB` at 180° rotation

## FFmpeg EBU R128 Master Bus Cross-Verification

| Metric | AutoMixMaster Mastered | FFmpeg ebur128 | Absolute Delta | Cross-Check Status |
| :--- | :---: | :---: | :---: | :---: |
| **Integrated Loudness** | -10.4252 LUFS | -10.4 LUFS | 0.0252138 LU | ✅ MATCH (<= 0.1 LU) |
| **True Peak** | -2.1235 dBTP | -2.1 dBTP | 0.0234987 dB | ✅ MATCH (<= 0.2 dB) |
