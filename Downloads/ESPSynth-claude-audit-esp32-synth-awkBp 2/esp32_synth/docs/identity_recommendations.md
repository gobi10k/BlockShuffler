# Identity Recommendations

Based on the audit findings, these **2–3 concrete, small-scope changes** would make the resonator and spectral processing more immediately accessible.

---

## Recommendation 1: Default Patch Include Resonator

### Current Behavior
- Resonator is disabled by default
- Default patch sounds like a generic subtractive synth
- User must send `Mx` command to enable resonator

### Proposed Change
- Enable resonator by default in `SynthEngine::SynthEngine()` constructor
- Set resonator to a musical profile (e.g., "harmonic" or "bell")
- Pre-configure resonator mix to ~30-50%

```cpp
// In SynthEngine constructor, change:
resonatorEnabled_(true),  // was false

// In init() or after construction:
resonator_.setProfile(ResonatorProfile::HARMONIC);
resonator_.setMix(0.4f);
```

### Expected Sonic Impact
- Default patch now has **spectral character** — a resonant, bell-like quality that distinguishes it from generic synths
- Users immediately hear what makes this synth unique
- Low risk: can be disabled with `Mx` if too intense

### Scope
- **Files:** `SynthEngine.cpp` (constructor + init)
- **Lines:** ~5 lines
- **Risk:** Low — simple enable flag

---

## Recommendation 2: Map Mod Wheel to Resonator Mix

### Current Behavior
- Mod wheel is connected to LFO depth by default
- No default modulation for resonator
- Users must discover resonator mix control manually

### Proposed Change
- Add secondary modulation: mod wheel also controls resonator mix
- In `ModMatrix::init()` or similar, add:

```cpp
modMatrix_.setSlot(modSlot++, ModSource::MOD_WHEEL, ModDest::RESONATOR_MIX, 0.5f);
```

Or in Voice/SynthEngine, apply mod wheel directly:
```cpp
float resonatorMix = resonatorMix_ * (1.0f + modWheel * 0.5f);  // modulate up to 150%
```

### Expected Sonic Impact
- **Performance expression** — pressing mod wheel brings in the resonator, creating a spectral "bloom" effect
- Intuitive: user naturally reaches for mod wheel to add character
- Makes the resonator feel like a performance feature, not a menu dive

### Scope
- **Files:** `ModMatrix.h`, `ModMatrix.cpp`, or `SynthEngine.cpp`
- **Lines:** ~5-10 lines
- **Risk:** Low — additive modulation, doesn't affect existing parameters

---

## Recommendation 3: Parallel Granular Mix (Replace Series Routing)

### Current Behavior
- Granular is mixed **in series** with voice:
  ```
  voice → (granularMix_) → output
         → (1-granularMix_) → output
  ```
- This means granular **replaces** voice energy rather than adding to it

### Proposed Change
- Change to **parallel add** mixing:
  ```
  voice → output (unchanged)
  granular → output (additive, scaled by granularMix_)
  ```

In `SynthEngine::processBlock()`:
```cpp
// Current (series):
left = left * (1.0f - granularMix_) + gran * granularMix_ * 0.7f;

// Proposed (parallel additive):
left = left + gran * granularMix_ * 0.3f;  // additive, not replacing
```

### Expected Sonic Impact
- Granular becomes a **textural layer** rather than a replacement
- User can have full-voice sound **plus** granular shimmer
- Enables "voice + texture" layered patches

### Scope
- **Files:** `SynthEngine.cpp` (processBlock)
- **Lines:** ~2-4 lines change
- **Risk:** Medium — changes mixing behavior; may need to adjust granularMix_ default to avoid clipping

---

## Summary Table

| # | Recommendation | Current | Proposed | Impact | Risk |
|---|---------------|---------|----------|---------|------|
| 1 | Default Resonator ON | Disabled | Enabled | Immediate spectral identity | Low |
| 2 | Mod Wheel → Resonator Mix | None | 0-50% mix | Performance expression | Low |
| 3 | Granular Parallel Mix | Series replace | Additive layer | Textural depth | Medium |

---

## Not Recommended (Out of Scope)

- Adding new waveforms — already complete
- Changing filter topology — functional
- Adding more LFO destinations — can be done later as polish

---

*Generated: April 2026*
*ESPSynth Resonant Spectral Engine*