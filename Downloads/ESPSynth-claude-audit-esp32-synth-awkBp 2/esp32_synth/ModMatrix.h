#ifndef MOD_MATRIX_H
#define MOD_MATRIX_H

#include "Config.h"

// ============================================================================
// MODULATION SOURCES
// ============================================================================

enum class ModSource : uint8_t {
    NONE = 0,
    LFO1,
    LFO2,
    ENV_FILTER,
    ENV_AMP,
    VELOCITY,
    MOD_WHEEL,
    NUM_SOURCES
};

extern const char* MOD_SOURCE_NAMES[];

// ============================================================================
// MODULATION DESTINATIONS
// ============================================================================

enum class ModDest : uint8_t {
    NONE = 0,
    OSC_PITCH,          // All oscillators
    OSC1_PITCH,
    OSC2_PITCH,
    OSC_MIX,            // Osc 1/2 balance
    FILTER_CUTOFF,
    FILTER_RESONANCE,
    AMP_LEVEL,
    LFO1_RATE,
    LFO2_RATE,
    DELAY_TIME,
    DELAY_FEEDBACK,
    CHORUS_DEPTH,
    RESONATOR_MIX,
    NUM_DESTINATIONS
};

extern const char* MOD_DEST_NAMES[];

// ============================================================================
// MODULATION SLOT
// ============================================================================

struct ModSlot {
    ModSource source;
    ModDest destination;
    float amount;       // -1 to +1
    bool enabled;
    
    ModSlot() : source(ModSource::NONE), destination(ModDest::NONE), 
                amount(0.0f), enabled(false) {}
};

// ============================================================================
// MODULATION MATRIX
// ============================================================================

constexpr uint8_t NUM_MOD_SLOTS = 8;

class ModMatrix {
public:
    ModMatrix();
    
    // Configure slots
    void setSlot(uint8_t slot, ModSource src, ModDest dest, float amount);
    void enableSlot(uint8_t slot, bool enabled);
    void clearSlot(uint8_t slot);
    void clearAll();
    
    ModSlot& getSlot(uint8_t slot);
    
    // Set source values (call each frame before processing)
    void setSourceValue(ModSource src, float value);
    
    // Get summed modulation for a destination
    float getModulation(ModDest dest) const;
    
    // Process all modulation (call once per sample)
    void process();

private:
    ModSlot slots_[NUM_MOD_SLOTS];
    float sourceValues_[static_cast<int>(ModSource::NUM_SOURCES)];
    float destValues_[static_cast<int>(ModDest::NUM_DESTINATIONS)];
};

#endif
