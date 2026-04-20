# Phase 3: Reliability & Polish — Peripheral Failure Audit

## Overview

This document audits the initialization sequence and identifies what happens when peripherals fail or are absent.

---

## Initialization Sequence (from `esp32_synth.ino`)

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // 1. Presets (NVS-based internal storage)
    presets.begin();
    
    // 2. Display (OLED)
    if (!display.init(&synth)) {
        Serial.println("Display init failed");  // <-- continues
    }
    
    // 3. Synth (Critical - audio engine)
    if (!synth.init()) {
        Serial.println("FATAL: Synth init failed");
        while (1) delay(1000);  // <-- hangs forever
    }
    
    // 4. SD Card
    sd.begin(SD_CS_PIN);  // no error check
    
    // 5. Controls
    controls.init(&synth);
    encNav.init();
    encVal.init();
    
    // 6. MIDI
    midi.begin(16, 17);  // no error check
    
    // 7. Presets
    if (!presets.loadPreset(0, initPreset)) {
        presets.loadFactoryPresets();
    }
    
    synth.start();
    display.start();
}
```

---

## Peripheral Failure Analysis

### 1. SynthEngine

| Scenario | Behavior | Impact |
|----------|----------|--------|
| I2S init fails | Enters infinite loop | Device non-functional |
| Audio task fails to start | Infinite loop | Device non-functional |

**Verdict:** Correct behavior. Without audio output, the device is useless.

---

### 2. DisplayManager (OLED)

| Scenario | Behavior | Impact |
|----------|----------|--------|
| Wire.begin() fails | Returns false, logs error | Audio continues |
| I2C scan fails | Returns false, logs error | Audio continues |
| Not connected | Returns false | Audio continues |

**Verdict:** Graceful fallback. Audio remains functional.

---

### 3. SD Card

| Scenario | Behavior | Impact |
|----------|----------|--------|
| SPI.begin() fails | Silent (no error check) | Audio continues |
| Card not present | Silent | Audio continues |
| Card fails mid-session | Silent | Audio continues |

**Verdict:** Silent failure. No user indication if SD is missing. Audio continues.

---

### 4. MIDI UART

| Scenario | Behavior | Impact |
|----------|----------|--------|
| UART init fails | Silent (no error check) | Audio continues |
| MIDI device not connected | Silent | Audio continues |
| MIDI device disconnects mid-session | Silent | Audio continues |

**Verdict:** Silent failure. MIDI simply won't work.

---

### 5. Encoders

| Scenario | Behavior | Impact |
|----------|----------|--------|
| Pin init fails | Silent (no error check) | Audio continues |
| Encoder not connected | Silent | Audio continues |
| Encoder disconnected mid-session | Silent | Audio continues |

**Verdict:** Silent failure. Parameter editing unavailable.

---

### 6. Analog Controls (Potentiometers)

| Scenario | Behavior | Impact |
|----------|----------|--------|
| ADC init fails | Logs error | Audio continues |
| Not connected | Silent | Audio continues |
| Pot disconnected mid-session | Silent | Audio continues |

**Verdict:** Silent failure unless explicitly checked.

---

## Summary Table

| Peripheral | Failure Handling | Audio Continues? | User Notified? |
|------------|----------------|-----------------|----------------|
| SynthEngine | Halts (correct) | N/A | Serial |
| DisplayManager | Continues | ✅ Yes | Serial only |
| SD Card | Continues | ✅ Yes | No |
| MIDI | Continues | ✅ Yes | No |
| Encoders | Continues | ✅ Yes | No |
| Analog | Continues | ✅ Yes | No |

---

## Identified Issues

1. **No SD card failure indication** — User doesn't know if card is missing
2. **No MIDI failure indication** — Silent unless debugging enabled
3. **Display failure only on Serial** — No LED or other visual indicator

---

## Recommendations (for proposed fixes)

1. Add an LED that blinks on boot and pulses during operation
2. Show SD/MIDI status on display if available
3. Add error logging for all hardware init failures

---

*Audit completed April 2026*