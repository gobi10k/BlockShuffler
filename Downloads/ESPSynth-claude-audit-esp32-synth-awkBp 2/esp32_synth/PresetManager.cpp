#include "PresetManager.h"
#include <Preferences.h>

Preferences preferences;

PresetManager::PresetManager() {
    for (int i = 0; i < NUM_PRESETS; i++) {
        slotUsed_[i] = false;
    }
}

void PresetManager::begin() {
    preferences.begin("synth", false);
    
    // Check which slots are used
    for (int i = 0; i < NUM_PRESETS; i++) {
        char key[8];
        snprintf(key, sizeof(key), "p%d", i);
        
        if (preferences.isKey(key)) {
            size_t len = preferences.getBytesLength(key);
            if (len == sizeof(PresetData)) {
                preferences.getBytes(key, &presetCache_[i], sizeof(PresetData));
                
                // Verify checksum
                uint32_t calc = calculateChecksum(presetCache_[i]);
                if (presetCache_[i].magic == PRESET_MAGIC && 
                    presetCache_[i].checksum == calc) {
                    slotUsed_[i] = true;
                }
            }
        }
    }
    
    Serial.printf("[Presets] Loaded, %d slots used\n", 
        [this]() { 
            int c = 0; 
            for (int i = 0; i < NUM_PRESETS; i++) if (slotUsed_[i]) c++; 
            return c; 
        }());
}

uint32_t PresetManager::calculateChecksum(const PresetData& preset) {
    uint32_t sum = 0;
    const uint8_t* data = (const uint8_t*)&preset;
    // Checksum everything except the checksum field itself
    size_t len = sizeof(PresetData) - sizeof(uint32_t);
    for (size_t i = 0; i < len; i++) {
        sum = sum * 31 + data[i];
    }
    return sum;
}

bool PresetManager::savePreset(uint8_t slot, const PresetData& preset) {
    if (slot >= NUM_PRESETS) return false;
    
    PresetData toSave = preset;
    toSave.magic = PRESET_MAGIC;
    toSave.checksum = calculateChecksum(toSave);
    
    char key[8];
    snprintf(key, sizeof(key), "p%d", slot);
    
    size_t written = preferences.putBytes(key, &toSave, sizeof(PresetData));
    if (written == sizeof(PresetData)) {
        presetCache_[slot] = toSave;
        slotUsed_[slot] = true;
        Serial.printf("[Presets] Saved to slot %d: %s\n", slot, toSave.name);
        return true;
    }
    
    Serial.println("[Presets] Save failed!");
    return false;
}

bool PresetManager::loadPreset(uint8_t slot, PresetData& preset) {
    if (slot >= NUM_PRESETS) return false;
    if (!slotUsed_[slot]) return false;
    
    preset = presetCache_[slot];
    Serial.printf("[Presets] Loaded slot %d: %s\n", slot, preset.name);
    return true;
}

bool PresetManager::deletePreset(uint8_t slot) {
    if (slot >= NUM_PRESETS) return false;
    
    char key[8];
    snprintf(key, sizeof(key), "p%d", slot);
    
    preferences.remove(key);
    slotUsed_[slot] = false;
    
    Serial.printf("[Presets] Deleted slot %d\n", slot);
    return true;
}

bool PresetManager::isSlotUsed(uint8_t slot) {
    if (slot >= NUM_PRESETS) return false;
    return slotUsed_[slot];
}

const char* PresetManager::getPresetName(uint8_t slot) {
    if (slot >= NUM_PRESETS || !slotUsed_[slot]) return "---";
    return presetCache_[slot].name;
}

PresetData PresetManager::getInitPreset() {
    PresetData p = {};
    
    strncpy(p.name, "Init", 15);
    
    // Oscillators
    p.osc1Wave = (uint8_t)Waveform::SAW;
    p.osc2Wave = (uint8_t)Waveform::SAW;
    p.osc1Detune = 0;
    p.osc2Detune = 7;
    p.oscMix = 50;
    p.osc1PW = 50;
    p.osc2PW = 50;
    
    // Filter
    p.filterCutoff = 2000;
    p.filterReso = 30;
    p.filterMode = (uint8_t)FilterMode::LOWPASS;
    p.filterEnvAmount = 50;
    
    // Amp ADSR
    p.ampAttack = 10;
    p.ampDecay = 100;
    p.ampSustain = 70;
    p.ampRelease = 300;
    
    // Filter ADSR
    p.filterAttack = 10;
    p.filterDecay = 200;
    p.filterSustain = 30;
    p.filterRelease = 500;
    
    // LFOs
    p.lfo1Wave = 0;  // Sine
    p.lfo1Rate = 100;  // 1.0 Hz
    p.lfo1Depth = 50;
    p.lfo2Wave = 1;  // Triangle
    p.lfo2Rate = 10;   // 0.1 Hz
    p.lfo2Depth = 50;
    
    // Arp off
    p.arpMode = 0;
    p.arpDivision = 4;
    p.arpTempo = 120;
    p.arpGate = 50;
    p.arpOctaves = 1;
    
    // Effects off
    p.effectFlags = 0;
    p.satDrive = 20;
    p.satType = 0;
    p.chorusRate = 50;
    p.chorusDepth = 50;
    p.delayTime = 250;
    p.delayFeedback = 40;
    p.delayMix = 30;
    
    p.glideTime = 0;
    p.masterVolume = 70;
    
    p.filterType = (uint8_t)VoiceFilterType::SVF;
    p.synthMode = (uint8_t)VoiceSynthMode::STANDARD;
    p.fmAmount = 10; // 1.0

    return p;
}

bool PresetManager::savePresetToSD(const char* filename, const PresetData& preset, SDManager& sd) {
    if (!sd.isAvailable()) return false;

    char path[64];
    snprintf(path, sizeof(path), "/presets/%s", filename);
    if (!strcasestr(path, ".sy")) {
        strncat(path, ".sy", sizeof(path) - strlen(path) - 1);
    }

    PresetData p = preset;
    p.checksum = calculateChecksum(p);

    return sd.writeFile(path, (uint8_t*)&p, sizeof(PresetData));
}

bool PresetManager::loadPresetFromSD(const char* filename, PresetData& preset, SDManager& sd) {
    if (!sd.isAvailable()) return false;

    char path[64];
    snprintf(path, sizeof(path), "/presets/%s", filename);
    if (!strcasestr(path, ".sy")) {
        strncat(path, ".sy", sizeof(path) - strlen(path) - 1);
    }

    PresetData p;
    if (!sd.readFile(path, (uint8_t*)&p, sizeof(PresetData))) {
        return false;
    }

    if (p.magic != PRESET_MAGIC || p.checksum != calculateChecksum(p)) {
        Serial.printf("[Preset] Invalid SD preset: %s\n", path);
        return false;
    }

    preset = p;
    return true;
}

void PresetManager::loadFactoryPresets() {
    // Preset 0: Init
    PresetData init = getInitPreset();
    savePreset(0, init);
    
    // Preset 1: Fat Bass
    PresetData bass = getInitPreset();
    strncpy(bass.name, "Fat Bass", 15);
    bass.osc1Wave = (uint8_t)Waveform::SAW;
    bass.osc2Wave = (uint8_t)Waveform::SQUARE;
    bass.osc2Detune = -12;  // Octave down... actually cents, so let's use mix
    bass.oscMix = 60;
    bass.filterCutoff = 800;
    bass.filterReso = 50;
    bass.filterEnvAmount = 70;
    bass.ampAttack = 5;
    bass.ampDecay = 200;
    bass.ampSustain = 60;
    bass.filterDecay = 300;
    bass.filterType = (uint8_t)VoiceFilterType::LADDER;
    savePreset(1, bass);
    
    // Preset 2: Supersaw Pad
    PresetData pad = getInitPreset();
    strncpy(pad.name, "Supersaw Pad", 15);
    pad.osc1Wave = (uint8_t)Waveform::SUPERSAW;
    pad.osc2Wave = (uint8_t)Waveform::SUPERSAW;
    pad.osc2Detune = 5;
    pad.filterCutoff = 3000;
    pad.filterReso = 20;
    pad.ampAttack = 500;
    pad.ampRelease = 1000;
    pad.lfo1Rate = 20;  // 0.2 Hz
    pad.effectFlags = 0x06;  // Chorus + Delay
    pad.chorusDepth = 70;
    pad.delayTime = 350;
    pad.delayFeedback = 35;
    savePreset(2, pad);
    
    // Preset 3: Acid Lead
    PresetData acid = getInitPreset();
    strncpy(acid.name, "Acid Lead", 15);
    acid.osc1Wave = (uint8_t)Waveform::SAW;
    acid.osc2Wave = (uint8_t)Waveform::SAW;
    acid.filterCutoff = 500;
    acid.filterReso = 85;
    acid.filterEnvAmount = 90;
    acid.filterAttack = 1;
    acid.filterDecay = 150;
    acid.filterSustain = 10;
    acid.ampDecay = 200;
    acid.glideTime = 50;
    savePreset(3, acid);
    
    // Preset 4: Arp Synth
    PresetData arp = getInitPreset();
    strncpy(arp.name, "Arp Synth", 15);
    arp.osc1Wave = (uint8_t)Waveform::PULSE;
    arp.osc1PW = 25;
    arp.filterCutoff = 1500;
    arp.filterEnvAmount = 40;
    arp.ampDecay = 150;
    arp.ampSustain = 0;
    arp.arpMode = 1;  // Up
    arp.arpDivision = 8;
    arp.arpOctaves = 2;
    arp.effectFlags = 0x04;  // Delay
    arp.delayTime = 187;  // Dotted eighth at 120bpm
    arp.delayFeedback = 50;
    savePreset(4, arp);

    // Preset 5: FM Lead
    PresetData fm = getInitPreset();
    strncpy(fm.name, "FM Lead", 15);
    fm.synthMode = (uint8_t)VoiceSynthMode::FM;
    fm.fmAmount = 80; // 8.0
    fm.osc1Wave = (uint8_t)Waveform::SINE;
    fm.osc2Wave = (uint8_t)Waveform::SINE;
    fm.filterCutoff = 5000;
    savePreset(5, fm);
    
    Serial.println("[Presets] Factory presets loaded");
}

void PresetManager::saveLastState(uint8_t lastPreset, float masterVolume) {
    preferences.putUChar("lastPreset", lastPreset);
    preferences.putFloat("masterVol", masterVolume);
}

void PresetManager::loadLastState(uint8_t& lastPreset, float& masterVolume) {
    lastPreset = preferences.getUChar("lastPreset", 0);
    masterVolume = preferences.getFloat("masterVol", 0.7f);
}
