#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include "Config.h"
#include "Wavetables.h"
#include "Filter.h"
#include "Arpeggiator.h"
#include "Voice.h"
#include "SDManager.h"

constexpr uint8_t NUM_PRESETS = 16;
constexpr uint32_t PRESET_MAGIC = 0x53594E54;  // "SYNT"

// Packed preset data structure
struct PresetData {
    uint32_t magic;
    char name[16];
    
    // Oscillators
    uint8_t osc1Wave;
    uint8_t osc2Wave;
    int8_t osc1Detune;
    int8_t osc2Detune;
    uint8_t oscMix;
    uint8_t osc1PW;
    uint8_t osc2PW;
    
    // Filter
    uint16_t filterCutoff;
    uint8_t filterReso;
    uint8_t filterMode;
    int8_t filterEnvAmount;
    
    // Amp envelope (in ms, capped at 10000)
    uint16_t ampAttack;
    uint16_t ampDecay;
    uint8_t ampSustain;
    uint16_t ampRelease;
    
    // Filter envelope
    uint16_t filterAttack;
    uint16_t filterDecay;
    uint8_t filterSustain;
    uint16_t filterRelease;
    
    // LFO 1
    uint8_t lfo1Wave;
    uint16_t lfo1Rate;  // x100 Hz
    uint8_t lfo1Depth;
    
    // LFO 2
    uint8_t lfo2Wave;
    uint16_t lfo2Rate;
    uint8_t lfo2Depth;
    
    // Arpeggiator
    uint8_t arpMode;
    uint8_t arpDivision;
    uint16_t arpTempo;
    uint8_t arpGate;
    uint8_t arpOctaves;
    
    // Effects enable flags
    uint8_t effectFlags;  // bit0=sat, bit1=chorus, bit2=delay
    
    // Effects params
    uint8_t satDrive;
    uint8_t satType;
    uint8_t chorusRate;
    uint8_t chorusDepth;
    uint16_t delayTime;
    uint8_t delayFeedback;
    uint8_t delayMix;
    
    // Glide
    uint16_t glideTime;
    
    // Master
    uint8_t masterVolume;
    
    // Extra Params
    uint8_t filterType;
    uint8_t synthMode;
    uint8_t fmAmount;
    uint8_t filterEnvVel;
    uint8_t filterKeyTrack;
    int8_t globalPan;
    uint8_t reserved[10];
    
    uint32_t checksum;
};

class PresetManager {
public:
    PresetManager();
    
    void begin();
    
    bool savePreset(uint8_t slot, const PresetData& preset);
    bool loadPreset(uint8_t slot, PresetData& preset);
    bool deletePreset(uint8_t slot);
    
    bool isSlotUsed(uint8_t slot);
    const char* getPresetName(uint8_t slot);
    
    // Factory presets
    void loadFactoryPresets();
    static PresetData getInitPreset();

    // SD Card storage
    bool savePresetToSD(const char* filename, const PresetData& preset, SDManager& sd);
    bool loadPresetFromSD(const char* filename, PresetData& preset, SDManager& sd);

    // Last state recall
    void saveLastState(uint8_t lastPreset, float masterVolume);
    void loadLastState(uint8_t& lastPreset, float& masterVolume);

private:
    uint32_t calculateChecksum(const PresetData& preset);
    
    PresetData presetCache_[NUM_PRESETS];
    bool slotUsed_[NUM_PRESETS];
};

#endif
