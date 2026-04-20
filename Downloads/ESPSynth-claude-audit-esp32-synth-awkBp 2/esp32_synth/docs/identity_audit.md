# Identity Audit Findings

## 1. Hearing the Modal Resonator as the Dominant Feature

**Current Behavior:**
- The modal resonator is **disabled by default** (`resonatorEnabled_ = false`)
- It is positioned **in series** after the voice mixer and granular exciter
- To enable: Serial command `Mx` (toggle) or `synth.setResonatorEnabled(true)`

**Steps from Default to Dominant Resonator Sound:**
1. Send serial command `Mx` to enable resonator
2. Optionally: `Mp<0-5>` to set profile (harmonic, bell, drum, tube, marimba, custom)
3. Optionally: `Md<0-99>` to set damping
4. Optionally: `Mm<0-99>` to set mix level

**Result:** At least **1 command** to enable, plus additional commands to shape the sound.

---

## 2. Hearing the Granular Exciter Prominently

**Current Behavior:**
- Granular exciter is **disabled by default** (`granularEnabled_ = false`)
- It is positioned **in series** before the resonator
- To enable: Serial command `Gx` (toggle) or `synth.setGranularEnabled(true)`

**Steps from Default to Prominent Granular:**
1. Send serial command `Gx` to enable granular
2. Optionally: `Gm<0-99>` to set mix level (default is unknown, needs checking)
3. Optionally: `Gd<1-100>` to set density (grains/sec)
4. Optionally: `Gt<ms>` to set grain duration

**Result:** At least **1 command** to enable, plus additional commands to shape.

---

## 3. FDN Reverb Routing

**Current Behavior:**
- The reverb is a **global effect in series** after the voice mix, granular, resonator, comb, and effects chain
- It processes **mono input** (`monoInput = (left + right) * 0.5f`) and outputs stereo
- Mix is controlled via `Rm<0-99>` (reverb send level)

**Impact on Resonator/Granular:**
- Because resonator and granular are **in series** before reverb, their output is summed into the reverb send
- This can **wash out detail** — fine for ambiance, but may obscure the spectral character of the resonator
- A **parallel send architecture** would preserve the resonator's clarity while adding reverb depth

---

## 4. Modulation Dead Ends

The following parameters are currently **static** (no modulation available):

| Parameter | Current Modulation | Could Benefit From |
|-----------|-------------------|-------------------|
| **Resonator Frequency** | None | LFO, velocity |
| **Resonator Damping** | None | LFO, velocity |
| **Resonator Brightness** | None | LFO |
| **Granular Density** | None | LFO, velocity |
| **Granular Duration** | None | LFO |
| **Granular Mix** | None | Mod wheel |
| **Filter Mode** | None | LFO (to sweep LP/HP/BP) |
| **Reverb Decay** | None | LFO (for evolving reverb) |
| **Reverb Mix** | None | Mod wheel |
| **Delay Time** | None | LFO (for warble) |
| **Chorus Rate** | None | LFO (self-modulation) |

---

## 5. Default Patch Character

**Initialization State:**
From `SynthEngine.cpp` constructor:
- Oscillators: SAW waveform
- Filter: SVF, LP mode, Cutoff 2000Hz, Resonance 0.3
- Amp Envelope: A=10ms, D=100ms, S=0.7, R=300ms
- Filter Envelope: A=10ms, D=200ms, S=0.3, R=500ms
- Effects: All disabled (saturation, chorus, delay, reverb, compressor)
- Resonator: **Disabled**
- Granular: **Disabled**
- LFO: 2Hz sine → Filter Cutoff

**What a User Hears on First Play:**
A **generic subtractive synth** sound — classic sawtooth through a low-pass filter with moderate resonance. The filter is gently modulated by a slow LFO. It's clean, musical, and entirely mainstream.

**What's Missing:**
- No resonator character
- No granular texture
- No reverb
- No stereo width from chorus/delay

---

## Summary

| Question | Answer |
|----------|--------|
| Resonator accessible? | Yes, but requires explicit enable (1+ commands) |
| Granular accessible? | Yes, but requires explicit enable (1+ commands) |
| Reverb routing | Series (global insert), can obscure spectral detail |
| Modulation coverage | Good for filter/amp, limited for spectral processors |
| Default identity | Generic subtractive — resonator/granular buried |

---

*Generated: April 2026*
*ESPSynth Resonant Spectral Engine*