#include "ModMatrix.h"
#include <string.h>

const char* MOD_SOURCE_NAMES[] = {
    "NONE", "LFO1", "LFO2", "F.ENV", "A.ENV", "VEL", "MW"
};

const char* MOD_DEST_NAMES[] = {
    "NONE", "PITCH", "O1.P", "O2.P", "O.MIX",
    "F.CUT", "F.RES", "AMP", "L1.RT", "L2.RT",
    "D.TIM", "D.FB", "CH.DP", "R.MIX"
};

ModMatrix::ModMatrix() {
    clearAll();
    memset(sourceValues_, 0, sizeof(sourceValues_));
    memset(destValues_, 0, sizeof(destValues_));
}

void ModMatrix::setSlot(uint8_t slot, ModSource src, ModDest dest, float amount) {
    if (slot >= NUM_MOD_SLOTS) return;
    
    slots_[slot].source = src;
    slots_[slot].destination = dest;
    slots_[slot].amount = constrain(amount, -1.0f, 1.0f);
    slots_[slot].enabled = true;
}

void ModMatrix::enableSlot(uint8_t slot, bool enabled) {
    if (slot >= NUM_MOD_SLOTS) return;
    slots_[slot].enabled = enabled;
}

void ModMatrix::clearSlot(uint8_t slot) {
    if (slot >= NUM_MOD_SLOTS) return;
    slots_[slot] = ModSlot();
}

void ModMatrix::clearAll() {
    for (int i = 0; i < NUM_MOD_SLOTS; i++) {
        slots_[i] = ModSlot();
    }
}

ModSlot& ModMatrix::getSlot(uint8_t slot) {
    return slots_[slot % NUM_MOD_SLOTS];
}

void ModMatrix::setSourceValue(ModSource src, float value) {
    int idx = static_cast<int>(src);
    if (idx < static_cast<int>(ModSource::NUM_SOURCES)) {
        sourceValues_[idx] = value;
    }
}

float ModMatrix::getModulation(ModDest dest) const {
    int idx = static_cast<int>(dest);
    if (idx < static_cast<int>(ModDest::NUM_DESTINATIONS)) {
        return destValues_[idx];
    }
    return 0.0f;
}

void ModMatrix::process() {
    // Clear destination values
    memset(destValues_, 0, sizeof(destValues_));
    
    // Sum all active modulation routings
    for (int i = 0; i < NUM_MOD_SLOTS; i++) {
        ModSlot& slot = slots_[i];
        
        if (!slot.enabled) continue;
        if (slot.source == ModSource::NONE) continue;
        if (slot.destination == ModDest::NONE) continue;
        
        int srcIdx = static_cast<int>(slot.source);
        int destIdx = static_cast<int>(slot.destination);
        
        float modValue = sourceValues_[srcIdx] * slot.amount;
        destValues_[destIdx] += modValue;
    }
}
