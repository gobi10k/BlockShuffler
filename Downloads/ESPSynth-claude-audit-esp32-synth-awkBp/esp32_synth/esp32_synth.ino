/**
 * ESP32 Polyphonic Synthesizer v5
 * 
 * Features:
 *   - 4-voice polyphony with voice stealing
 *   - 7 waveforms per oscillator  
 *   - 2 filter types per voice: SVF, Moog Ladder
 *   - Global Modal Resonator and Comb filter
 *   - Arpeggiator with 6 modes
 *   - Hardware MIDI input
 *   - 16 preset slots
 *   - Effects: Delay, Saturation, Chorus, FDN Reverb, Compressor
 *   - Modulation matrix
 * 
 * Memory optimized for ESP32 DRAM constraints
 */

#include "Config.h"
#include "SynthEngine.h"
#include "MIDIHandler.h"
#include "PresetManager.h"
#include "DisplayManager.h"
#include "SynthesisTests.h"
#include "AnalogControls.h"
#include "HardwareEncoder.h"
#include "SDManager.h"

SynthEngine synth;
MIDIHandler midi;
PresetManager presets;
DisplayManager display;
AnalogControls controls;
SDManager sd;
HardwareEncoder encNav(ENC1_A_PIN, ENC1_B_PIN, ENC1_SW_PIN);
HardwareEncoder encVal(ENC2_A_PIN, ENC2_B_PIN, ENC2_SW_PIN);

// ============================================================================
// STATE
// ============================================================================

bool satEnabled = false;
bool chorusEnabled = false;
bool delayEnabled = false;
int selectedLFO = 0;
bool autoStats = false;
uint32_t lastHeartbeatBlock = 0;

// Deferred MIDI program change: set in the MIDI callback, consumed in loop()
// to avoid blocking the MIDI/clock path with NVS flash access (10-100 ms).
volatile bool presetPending_ = false;
volatile uint8_t pendingPresetProgram_ = 0;

// ============================================================================
// MIDI CALLBACKS
// ============================================================================

void onMIDINoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    synth.noteOn(note, vel);
}

void onMIDINoteOff(uint8_t ch, uint8_t note) {
    synth.noteOff(note);
}

void onMIDICC(uint8_t ch, uint8_t cc, uint8_t val) {
    switch (cc) {
        case MIDI_CC::MOD_WHEEL:
            synth.getLFO(0).setDepth(val / 127.0f);
            break;
        case MIDI_CC::VOLUME:
            synth.setMasterVolume(val / 127.0f);
            break;
        case MIDI_CC::PAN:
            synth.setGlobalPan((val - 64) / 64.0f);
            break;
        case MIDI_CC::EXPRESSION:
            // Could map to something else, for now just volume scale
            break;
        case MIDI_CC::FILTER_CUTOFF:
            synth.setFilterCutoff(20.0f + val * 156.0f);  // 20-20000 Hz approx
            break;
        case MIDI_CC::FILTER_RESO:
            synth.setFilterResonance(val / 127.0f);
            break;
        case MIDI_CC::ATTACK:
            synth.setAmpADSR(val * 0.02f, -1, -1, -1);  // -1 = unchanged
            synth.setFilterADSR(val * 0.02f, -1, -1, -1);
            break;
        case MIDI_CC::DECAY:
            synth.setAmpADSR(-1, val * 0.02f, -1, -1);
            synth.setFilterADSR(-1, val * 0.02f, -1, -1);
            break;
        case MIDI_CC::SUSTAIN_LEVEL:
            synth.setAmpADSR(-1, -1, val / 127.0f, -1);
            break;
        case MIDI_CC::RELEASE:
            synth.setAmpADSR(-1, -1, -1, val * 0.02f);
            synth.setFilterADSR(-1, -1, -1, val * 0.02f);
            break;
        case MIDI_CC::FILTER_KEY_TRACK:
            synth.setFilterKeyTracking(val / 127.0f);
            break;
        case MIDI_CC::FILTER_ENV_VEL:
            synth.setFilterEnvVelocity(val / 127.0f);
            break;
        case MIDI_CC::REVERB_SEND:
            synth.getReverb().setMix(val / 127.0f);
            if (val > 0) synth.getReverb().setEnabled(true);
            break;
        case MIDI_CC::DELAY_SEND:
            synth.getEffects().delay.setMix(val / 127.0f);
            break;
        case MIDI_CC::CHORUS_SEND:
            synth.getEffects().chorus.setMix(val / 127.0f);
            break;
        case MIDI_CC::OSC_MIX:
            synth.setOscMix(val / 127.0f);
            break;
        case MIDI_CC::OSC1_WAVE:
            synth.setOscWaveform(0, (Waveform)(val % 7));
            break;
        case MIDI_CC::OSC2_WAVE:
            synth.setOscWaveform(1, (Waveform)(val % 7));
            break;
        case MIDI_CC::OSC2_DETUNE:
            synth.setOscDetune(1, (val - 64) * 0.5f);
            break;
        case MIDI_CC::SYNTH_MODE:
            synth.setSynthMode((VoiceSynthMode)(val % 4));
            break;
        case MIDI_CC::FM_AMOUNT:
            synth.setFMAmount(val / 5.0f);
            break;
        case MIDI_CC::FILTER_TYPE:
            synth.setFilterType((VoiceFilterType)(val % 2));
            break;
        case MIDI_CC::ALL_NOTES_OFF:
            synth.allNotesOff();
            break;
    }
}

void onMIDIPitchBend(uint8_t ch, int16_t val) {
    synth.setPitchBend(val);
}

void onMIDIProgramChange(uint8_t ch, uint8_t program) {
    // Do not call loadPreset() here — NVS flash takes 10-100 ms and would
    // starve the MIDI clock callback, overflowing the UART FIFO.
    pendingPresetProgram_ = program;
    presetPending_ = true;
}

void onMIDIClock() {
    synth.getArp().clockTick();
}

// ============================================================================
// PRESET APPLY/CREATE
// ============================================================================

void applyPreset(const PresetData& p) {
    synth.setOscWaveform(0, (Waveform)p.osc1Wave);
    synth.setOscWaveform(1, (Waveform)p.osc2Wave);
    synth.setOscDetune(0, p.osc1Detune);
    synth.setOscDetune(1, p.osc2Detune);
    synth.setOscMix(p.oscMix / 100.0f);
    
    synth.setFilterCutoff(p.filterCutoff);
    synth.setFilterResonance(p.filterReso / 100.0f);
    synth.setFilterMode((FilterMode)p.filterMode);
    synth.setFilterType((VoiceFilterType)p.filterType);
    synth.setSynthMode((VoiceSynthMode)p.synthMode);
    synth.setFMAmount(p.fmAmount / 10.0f);  // Scale 0-255 to 0-25.5
    synth.setFilterEnvAmount(p.filterEnvAmount / 100.0f);
    synth.setFilterEnvVelocity(p.filterEnvVel / 100.0f);
    synth.setFilterKeyTracking(p.filterKeyTrack / 100.0f);
    
    synth.setAmpADSR(p.ampAttack / 1000.0f, p.ampDecay / 1000.0f,
                    p.ampSustain / 100.0f, p.ampRelease / 1000.0f);
    synth.setFilterADSR(p.filterAttack / 1000.0f, p.filterDecay / 1000.0f,
                       p.filterSustain / 100.0f, p.filterRelease / 1000.0f);
    
    synth.getLFO(0).setWaveform((LFOWaveform)p.lfo1Wave);
    synth.getLFO(0).setFrequency(p.lfo1Rate / 100.0f);
    synth.getLFO(0).setDepth(p.lfo1Depth / 100.0f);
    synth.getLFO(1).setWaveform((LFOWaveform)p.lfo2Wave);
    synth.getLFO(1).setFrequency(p.lfo2Rate / 100.0f);
    synth.getLFO(1).setDepth(p.lfo2Depth / 100.0f);
    
    synth.getArp().setMode((ArpMode)p.arpMode);
    synth.getArp().setDivision(p.arpDivision);
    synth.getArp().setTempo(p.arpTempo);
    synth.getArp().setGateLength(p.arpGate / 100.0f);
    synth.getArp().setOctaveRange(p.arpOctaves);
    
    satEnabled = p.effectFlags & 0x01;
    chorusEnabled = p.effectFlags & 0x02;
    delayEnabled = p.effectFlags & 0x04;
    synth.getEffects().setEnabled(satEnabled, chorusEnabled, delayEnabled);
    
    synth.getEffects().saturation.setDrive(p.satDrive / 10.0f);
    synth.getEffects().saturation.setType((SaturationType)p.satType);
    synth.getEffects().chorus.setRate(p.chorusRate / 100.0f);
    synth.getEffects().chorus.setDepth(p.chorusDepth / 100.0f);
    synth.getEffects().delay.setTime(p.delayTime / 1000.0f);
    synth.getEffects().delay.setFeedback(p.delayFeedback / 100.0f);
    synth.getEffects().delay.setMix(p.delayMix / 100.0f);
    
    synth.setGlideTime(p.glideTime);
    synth.setMasterVolume(p.masterVolume / 100.0f);
    synth.setGlobalPan(p.globalPan / 100.0f);
    
    Serial.printf("Loaded: %s\n", p.name);
}

PresetData createPresetFromCurrent(const char* name) {
    PresetData p = PresetManager::getInitPreset();
    strncpy(p.name, name, 15);
    
    p.osc1Wave = (uint8_t)synth.getOscWaveform(0);
    p.osc2Wave = (uint8_t)synth.getOscWaveform(1);
    p.osc1Detune = (int8_t)synth.getOscDetune(0);
    p.osc2Detune = (int8_t)synth.getOscDetune(1);
    p.oscMix = (uint8_t)(synth.getOscMix() * 100.0f);

    p.filterCutoff = (uint16_t)synth.getFilterCutoff();
    p.filterReso = (uint8_t)(synth.getFilterResonance() * 100.0f);
    p.filterMode = (uint8_t)synth.getFilterMode();
    p.filterType = (uint8_t)synth.getFilterType();
    p.synthMode = (uint8_t)synth.getSynthMode();
    p.fmAmount = (uint8_t)(synth.getFMAmount() * 10.0f);
    p.filterEnvVel = (uint8_t)(synth.getFilterEnvVelocity() * 100.0f);
    p.filterKeyTrack = (uint8_t)(synth.getFilterKeyTracking() * 100.0f);
    p.globalPan = (int8_t)(synth.getGlobalPan() * 100.0f);
    
    p.effectFlags = (satEnabled ? 1 : 0) | (chorusEnabled ? 2 : 0) | (delayEnabled ? 4 : 0);
    
    return p;
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);
    delay(1000);

    Serial.println("\n=== ESP32 Synth v5 - Resonant Spectral Engine ===\n");

    // Initialize ADC FIRST - before any other libraries that might auto-init legacy ADC
    AnalogControls::initADC();

    // Initialize subsystems
    presets.begin();

    if (!display.init(&synth)) {
        Serial.println("Display init failed");
    }

    if (!synth.init()) {
        Serial.println("FATAL: Synth init failed");
        while (1) delay(1000);
    }

    // Setup SD card
    sd.begin(SD_CS_PIN);

    // Setup controls
    controls.init(&synth);
    encNav.init();
    encVal.init();

    // Setup MIDI
    midi.begin(16, 17);  // RX=16, TX=17
    midi.setNoteOnCallback(onMIDINoteOn);
    midi.setNoteOffCallback(onMIDINoteOff);
    midi.setCCCallback(onMIDICC);
    midi.setProgramChangeCallback(onMIDIProgramChange);
    midi.setPitchBendCallback(onMIDIPitchBend);
    midi.setClockCallback(onMIDIClock);
    
    // Load init preset or factory if first boot
    PresetData initPreset;
    if (!presets.loadPreset(0, initPreset)) {
        Serial.println("Loading factory presets...");
        presets.loadFactoryPresets();
        presets.loadPreset(0, initPreset);
    }
    applyPreset(initPreset);
    
    synth.start();
    display.start();
    
    printHelp();
    Serial.println("\nReady. Type ? for help.\n");
}

// ============================================================================
// HELP
// ============================================================================

void printHelp() {
    Serial.println("=== ESP32 SYNTH v5 COMMANDS ===");
    Serial.println("");
    Serial.println("-- Notes --");
    Serial.println("  n<midi>    Note on (n60 = C4)");
    Serial.println("  x<midi>    Note off");
    Serial.println("  X          All notes off");
    Serial.println("");
    Serial.println("-- Oscillators --");
    Serial.println("  w1<0-6>    Osc1 wave (sin/saw/sqr/tri/pls/sup/noi)");
    Serial.println("  w2<0-6>    Osc2 wave");
    Serial.println("  d1<cents>  Osc1 detune");
    Serial.println("  d2<cents>  Osc2 detune");
    Serial.println("  om<0-99>   Osc mix %");
    Serial.println("  sm<0-3>    Synth mode (std/fm/sync/ring)");
    Serial.println("  sa<val>    FM amount");
    Serial.println("");
    Serial.println("-- Filter --");
    Serial.println("  ft<0-3>    Type (SVF/Ladder/Resonator/Comb)");
    Serial.println("  c<hz>      Cutoff");
    Serial.println("  r<0-99>    Resonance %");
    Serial.println("  fm<0-3>    Mode (lp/hp/bp/notch)");
    Serial.println("  fe<-99-99> Env amount %");
    Serial.println("");
    Serial.println("-- Resonator (ft2) --");
    Serial.println("  rp<0-5>    Profile (harm/bell/drum/tube/marimba/custom)");
    Serial.println("  rd<0-99>   Damping %");
    Serial.println("  rb<0-99>   Brightness %");
    Serial.println("");
    Serial.println("-- Comb (ft3) --");
    Serial.println("  cm<0-3>    Mode (fb/ff/allpass/karplus)");
    Serial.println("  cd<0-99>   Damping %");
    Serial.println("");
    Serial.println("-- Envelopes --");
    Serial.println("  aa/ad/as/ar  Amp ADSR (ms/ms/%/ms)");
    Serial.println("  fa/fd/fs/fr  Filter ADSR");
    Serial.println("");
    Serial.println("-- Arpeggiator --");
    Serial.println("  am<0-6>    Mode (off/up/dn/ud/du/rnd/ord)");
    Serial.println("  at<bpm>    Tempo");
    Serial.println("  ad<div>    Division (1/2/4/8/16)");
    Serial.println("  ag<0-99>   Gate length %");
    Serial.println("  ao<1-4>    Octave range");
    Serial.println("");
    Serial.println("-- Euclidean --");
    Serial.println("  Ex         Toggle Euclidean seq");
    Serial.println("  Es<1-32>   Steps");
    Serial.println("  Ep<n>      Pulses");
    Serial.println("  Er<n>      Rotation");
    Serial.println("  Ew<0-99>   Swing %");
    Serial.println("  Eb<bpm>    Tempo");
    Serial.println("");
    Serial.println("-- LFO --");
    Serial.println("  l<1-2>     Select LFO");
    Serial.println("  lf<hz>     Frequency");
    Serial.println("  lw<0-5>    Waveform");
    Serial.println("  ld<0-99>   Depth %");
    Serial.println("");
    Serial.println("-- Granular --");
    Serial.println("  Gx         Toggle granular");
    Serial.println("  Gm<0-99>   Mix %");
    Serial.println("  Gd<1-100>  Density (grains/sec)");
    Serial.println("  Gt<ms>     Duration (5-500ms)");
    Serial.println("  Gs<0-4>    Source (noi/sin/imp/tri/dust)");
    Serial.println("");
    Serial.println("-- Reverb --");
    Serial.println("  Rx         Toggle reverb");
    Serial.println("  Rd<0.1-15> Decay (seconds)");
    Serial.println("  Rs<0-99>   Size %");
    Serial.println("  Rh<0-99>   Damping (HF) %");
    Serial.println("  Rm<0-99>   Mix %");
    Serial.println("  Rp<ms>     Pre-delay");
    Serial.println("");
    Serial.println("-- Effects --");
    Serial.println("  sx/Cx/dx   Toggle sat/chorus/delay");
    Serial.println("  dt/df/dm   Delay time/fb/mix");
    Serial.println("  Kx         Toggle compressor");
    Serial.println("  Kt/Kr/Ka   Comp thresh/ratio/attack");
    Serial.println("");
    Serial.println("-- SD Card --");
    Serial.println("  Dl         List SD files");
    Serial.println("  Ds<slot>   Save preset to SD");
    Serial.println("  DL<slot>   Load preset from SD");
    Serial.println("");
    Serial.println("-- Presets --");
    Serial.println("  P          List internal presets");
    Serial.println("  P<0-15>    Load internal preset");
    Serial.println("  PS<0-15>   Save to internal slot");
    Serial.println("  PF         Load factory presets");
    Serial.println("");
    Serial.println("-- Other --");
    Serial.println("  gl<ms>     Glide time");
    Serial.println("  v<0-99>    Master volume %");
    Serial.println("  z          Print CPU statistics");
    Serial.println("  p          Toggle auto CPU stats");
    Serial.println("  pn/pp      Display: Next/Prev Page");
    Serial.println("  t          Run internal tests");
    Serial.println("  S          Stress test (4 notes, FX on)");
    Serial.println("  k          Isolated module stress test (sequential)");
    Serial.println("  V          List active voices status");
    Serial.println("  ?          Help");
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

void processCommand(const String& cmd) {
    if (cmd.length() == 0) return;
    
    char c0 = cmd.charAt(0);
    char c1 = (cmd.length() > 1) ? cmd.charAt(1) : 0;
    
    int numStart = 1;
    if (c1 && !isDigit(c1) && c1 != '-' && c1 != '.') numStart = 2;
    float value = 0;
    if ((int)cmd.length() > numStart) {
        value = cmd.substring(numStart).toFloat();
    }
    
    switch (c0) {
        // === NOTES ===
        case 'n':
            synth.noteOn((uint8_t)value, 100);
            Serial.printf("Note ON: %d\n", (int)value);
            break;
        case 'x':
            synth.noteOff((uint8_t)value);
            Serial.printf("Note OFF: %d\n", (int)value);
            break;
        case 'X':
            synth.allNotesOff();
            Serial.println("All notes off");
            break;
            
        // === OSCILLATORS ===
        case 'w':
            if (c1 == '1') {
                synth.setOscWaveform(0, (Waveform)((int)value % 7));
                Serial.printf("Osc1: %s\n", WAVEFORM_NAMES[(int)value % 7]);
            } else if (c1 == '2') {
                synth.setOscWaveform(1, (Waveform)((int)value % 7));
                Serial.printf("Osc2: %s\n", WAVEFORM_NAMES[(int)value % 7]);
            }
            break;
        case 'd':
            if (c1 == '1') {
                synth.setOscDetune(0, value);
                Serial.printf("Osc1 detune: %.0f\n", value);
            } else if (c1 == '2') {
                synth.setOscDetune(1, value);
                Serial.printf("Osc2 detune: %.0f\n", value);
            } else if (c1 == 'x') {
                delayEnabled = !delayEnabled;
                synth.getEffects().setEnabled(satEnabled, chorusEnabled, delayEnabled);
                Serial.printf("Delay: %s\n", delayEnabled ? "ON" : "OFF");
            } else if (c1 == 't') {
                synth.getEffects().delay.setTime(value / 1000.0f);
                Serial.printf("Delay: %.0fms\n", value);
            } else if (c1 == 'f') {
                synth.getEffects().delay.setFeedback(value / 100.0f);
                Serial.printf("Delay FB: %.0f%%\n", value);
            } else if (c1 == 'm') {
                synth.getEffects().delay.setMix(value / 100.0f);
                Serial.printf("Delay mix: %.0f%%\n", value);
            }
            break;
        case 'o':
            if (c1 == 'm') {
                synth.setOscMix(value / 100.0f);
                Serial.printf("Osc mix: %.0f%%\n", value);
            }
            break;
            
        // === FILTER ===
        case 'c':
            synth.setFilterCutoff(value);
            Serial.printf("Cutoff: %.0fHz\n", value);
            break;
        case 'r':
            if (c1 == 'p') {
                synth.getResonator().setProfile((ResonatorProfile)((int)value % 6));
                Serial.printf("Res profile: %d\n", (int)value);
            } else if (c1 == 'd') {
                synth.getResonator().setDamping(value / 100.0f);
                Serial.printf("Res damp: %.0f%%\n", value);
            } else if (c1 == 'b') {
                synth.getResonator().setBrightness(value / 100.0f);
                Serial.printf("Res brightness: %.0f%%\n", value);
            } else {
                synth.setFilterResonance(value / 100.0f);
                Serial.printf("Resonance: %.0f%%\n", value);
            }
            break;
        case 'f':
            if (c1 == 't') {
                synth.setFilterType((VoiceFilterType)((int)value % 2));
                Serial.printf("Filter type: %s\n", VOICE_FILTER_NAMES[(int)value % 2]);
            } else if (c1 == 'm') {
                synth.setFilterMode((FilterMode)((int)value % 4));
                Serial.printf("Filter: %s\n", FILTER_MODE_NAMES[(int)value % 4]);
            } else if (c1 == 'e') {
                synth.setFilterEnvAmount(value / 100.0f);
                Serial.printf("Filter env: %.0f%%\n", value);
            } else if (c1 == 'a') {
                synth.setFilterADSR(value / 1000.0f, -1, -1, -1);
                Serial.printf("Flt A: %.0fms\n", value);
            } else if (c1 == 'd') {
                synth.setFilterADSR(-1, value / 1000.0f, -1, -1);
                Serial.printf("Flt D: %.0fms\n", value);
            } else if (c1 == 's') {
                synth.setFilterADSR(-1, -1, value / 100.0f, -1);
                Serial.printf("Flt S: %.0f%%\n", value);
            } else if (c1 == 'r') {
                synth.setFilterADSR(-1, -1, -1, value / 1000.0f);
                Serial.printf("Flt R: %.0fms\n", value);
            }
            break;
            
        // === GLOBAL RESONATOR (M) ===
        case 'M':
            if (c1 == 'x') {
                synth.setResonatorEnabled(!synth.isResonatorEnabled());
                Serial.printf("Resonator: %s\n", synth.isResonatorEnabled() ? "ON" : "OFF");
            } else if (c1 == 'p') {
                synth.getResonator().setProfile((ResonatorProfile)((int)value % 6));
                Serial.printf("Res profile: %d\n", (int)value);
            } else if (c1 == 'd') {
                synth.getResonator().setDamping(value / 100.0f);
                Serial.printf("Res damp: %.0f%%\n", value);
            } else if (c1 == 'm') {
                synth.getResonator().setMix(value / 100.0f);
                Serial.printf("Res mix: %.0f%%\n", value);
            }
            break;
            
        // === GLOBAL COMB (B) ===
        case 'B':
            if (c1 == 'x') {
                synth.setCombEnabled(!synth.isCombEnabled());
                Serial.printf("Comb: %s\n", synth.isCombEnabled() ? "ON" : "OFF");
            } else if (c1 == 'm') {
                synth.getComb().setMode((CombMode)((int)value % 4));
                Serial.printf("Comb mode: %d\n", (int)value);
            } else if (c1 == 'd') {
                synth.getComb().setDamping(value / 100.0f);
                Serial.printf("Comb damp: %.0f%%\n", value);
            } else if (c1 == 'f') {
                synth.getComb().setFeedback(value / 100.0f);
                Serial.printf("Comb fb: %.0f%%\n", value);
            }
            break;
            
        // === AMP ENVELOPE ===
        case 'a':
            if (c1 == 'a') {
                synth.setAmpADSR(value / 1000.0f, -1, -1, -1);
                Serial.printf("Amp A: %.0fms\n", value);
            } else if (c1 == 'd') {
                synth.setAmpADSR(-1, value / 1000.0f, -1, -1);
                Serial.printf("Amp D: %.0fms\n", value);
            } else if (c1 == 's') {
                synth.setAmpADSR(-1, -1, value / 100.0f, -1);
                Serial.printf("Amp S: %.0f%%\n", value);
            } else if (c1 == 'r') {
                synth.setAmpADSR(-1, -1, -1, value / 1000.0f);
                Serial.printf("Amp R: %.0fms\n", value);
            } else if (c1 == 'm') {
                synth.getArp().setMode((ArpMode)((int)value % 7));
                Serial.printf("Arp: %s\n", ARP_MODE_NAMES[(int)value % 7]);
            } else if (c1 == 't') {
                synth.getArp().setTempo(value);
                Serial.printf("Arp tempo: %.0f\n", value);
            } else if (c1 == 'g') {
                synth.getArp().setGateLength(value / 100.0f);
                Serial.printf("Arp gate: %.0f%%\n", value);
            } else if (c1 == 'o') {
                synth.getArp().setOctaveRange((uint8_t)value);
                Serial.printf("Arp oct: %.0f\n", value);
            }
            break;
            
        // === LFO ===
        case 'l':
            if (c1 == 'f') {
                synth.getLFO(selectedLFO).setFrequency(value);
                Serial.printf("LFO%d: %.2fHz\n", selectedLFO + 1, value);
            } else if (c1 == 'w') {
                synth.getLFO(selectedLFO).setWaveform((LFOWaveform)((int)value % 6));
                Serial.printf("LFO%d wave: %d\n", selectedLFO + 1, (int)value % 6);
            } else if (c1 == 'd') {
                synth.getLFO(selectedLFO).setDepth(value / 100.0f);
                Serial.printf("LFO%d depth: %.0f%%\n", selectedLFO + 1, value);
            } else {
                selectedLFO = ((int)value - 1) % 2;
                Serial.printf("Selected LFO%d\n", selectedLFO + 1);
            }
            break;
            
        // === EFFECTS ===
        case 's':
            if (c1 == 'x') {
                satEnabled = !satEnabled;
                synth.getEffects().setEnabled(satEnabled, chorusEnabled, delayEnabled);
                Serial.printf("Saturation: %s\n", satEnabled ? "ON" : "OFF");
            } else if (c1 == 'd') {
                synth.getEffects().saturation.setDrive(value);
                Serial.printf("Drive: %.1f\n", value);
            } else if (c1 == 'm') {
                synth.setSynthMode((VoiceSynthMode)((int)value % 4));
                Serial.printf("Synth mode: %s\n", VOICE_SYNTH_MODE_NAMES[(int)value % 4]);
            } else if (c1 == 'a') {
                synth.setFMAmount(value);
                Serial.printf("FM amount: %.1f\n", value);
            }
            break;
        case 'C':
            if (c1 == 'x') {
                chorusEnabled = !chorusEnabled;
                synth.getEffects().setEnabled(satEnabled, chorusEnabled, delayEnabled);
                Serial.printf("Chorus: %s\n", chorusEnabled ? "ON" : "OFF");
            } else if (c1 == 'r') {
                synth.getEffects().chorus.setRate(value);
                Serial.printf("Chorus rate: %.2fHz\n", value);
            } else if (c1 == 'd') {
                synth.getEffects().chorus.setDepth(value / 100.0f);
                Serial.printf("Chorus depth: %.0f%%\n", value);
            }
            break;
        case 'K':
            if (c1 == 'x') {
                synth.getCompressor().setEnabled(!synth.getCompressor().isEnabled());
                Serial.printf("Compressor: %s\n", synth.getCompressor().isEnabled() ? "ON" : "OFF");
            } else if (c1 == 't') {
                synth.getCompressor().setThreshold(value);
                Serial.printf("Comp thresh: %.1fdB\n", value);
            } else if (c1 == 'r') {
                synth.getCompressor().setRatio(value);
                Serial.printf("Comp ratio: %.1f:1\n", value);
            } else if (c1 == 'a') {
                synth.getCompressor().setAttack(value);
                Serial.printf("Comp attack: %.1fms\n", value);
            }
            break;
            
        // === REVERB ===
        case 'R':
            if (c1 == 'x') {
                synth.getReverb().setEnabled(!synth.getReverb().isEnabled());
                Serial.printf("Reverb: %s\n", synth.getReverb().isEnabled() ? "ON" : "OFF");
            } else if (c1 == 'd') {
                synth.getReverb().setDecay(value);
                Serial.printf("Reverb decay: %.1fs\n", value);
            } else if (c1 == 's') {
                synth.getReverb().setSize(value / 100.0f);
                Serial.printf("Reverb size: %.0f%%\n", value);
            } else if (c1 == 'h') {
                synth.getReverb().setDamping(value / 100.0f);
                Serial.printf("Reverb damp: %.0f%%\n", value);
            } else if (c1 == 'm') {
                synth.getReverb().setMix(value / 100.0f);
                Serial.printf("Reverb mix: %.0f%%\n", value);
            } else if (c1 == 'p') {
                synth.getReverb().setPreDelay(value);
                Serial.printf("Reverb pre: %.0fms\n", value);
            }
            break;
            
        // === DISK (SD CARD) ===
        case 'D':
            if (c1 == 'l') {
                sd.listFiles();
            } else if (c1 == 's') {
                int slot = (int)value;
                PresetData p = createPresetFromCurrent("SD_Save");
                char filename[32];
                snprintf(filename, sizeof(filename), "preset_%d", slot);
                if (presets.savePresetToSD(filename, p, sd)) {
                    Serial.printf("Saved to SD: %s\n", filename);
                }
            } else if (c1 == 'L') {
                int slot = (int)value;
                PresetData p;
                char filename[32];
                snprintf(filename, sizeof(filename), "preset_%d", slot);
                if (presets.loadPresetFromSD(filename, p, sd)) {
                    applyPreset(p);
                    Serial.printf("Loaded from SD: %s\n", filename);
                }
            }
            break;

        // === PRESETS ===
        case 'P':
            if (c1 == 'F') {
                presets.loadFactoryPresets();
            } else if (c1 == 'S') {
                int slot = cmd.substring(2).toInt();
                PresetData p = createPresetFromCurrent("User");
                presets.savePreset(slot, p);
            } else if (isDigit(c1)) {
                int slot = cmd.substring(1).toInt();
                PresetData p;
                if (presets.loadPreset(slot, p)) {
                    applyPreset(p);
                } else {
                    Serial.println("Empty slot");
                }
            } else {
                Serial.println("Presets:");
                for (int i = 0; i < NUM_PRESETS; i++) {
                    Serial.printf("  %2d: %s\n", i, presets.getPresetName(i));
                }
            }
            break;
            
        // === PROFILER / TESTS ===
        case 'z':
            synth.printCPUStats();
            break;
        case 'p':
            if (c1 == 'n') {
                display.nextPage();
                Serial.println("Display: Next Page");
            } else if (c1 == 'p') {
                display.prevPage();
                Serial.println("Display: Prev Page");
            } else {
                autoStats = !autoStats;
                Serial.printf("Auto stats: %s\n", autoStats ? "ON" : "OFF");
            }
            break;
        case 't':
            SynthesisTests::runAll();
            break;
        case 'k':
            Serial.println("Starting Sequential Module Stress Test (Wait for it)...");
            Serial.flush();

            synth.allNotesOff();
            synth.getEffects().setEnabled(false, false, false);
            synth.getReverb().setEnabled(false);
            synth.getCompressor().setEnabled(false);
            synth.setResonatorEnabled(false);
            synth.setCombEnabled(false);
            synth.setGranularEnabled(false);
            delay(1000);

            Serial.printf("Idle CPU: %.1f%%\n", synth.getCPUPercent());
            Serial.println("1. Triggering 4 voices (Clean)...");
            Serial.flush();
            for (int i = 0; i < NUM_VOICES; i++) {
                Serial.printf("   - Voice %d (Note %d)\n", i, 48 + i * 5);
                Serial.flush();
                synth.noteOn(48 + i * 5, 80);
                delay(200);
            }
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("2. Enabling Saturation...");
            Serial.flush();
            synth.getEffects().saturation.setMix(1.0f);
            synth.getEffects().setEnabled(true, false, false);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("2b. Enabling Chorus...");
            Serial.flush();
            synth.getEffects().setEnabled(true, true, false);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("2c. Enabling Delay...");
            Serial.flush();
            synth.getEffects().setEnabled(true, true, true);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("3. Adding Reverb...");
            Serial.flush();
            synth.getReverb().setEnabled(true);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("4. Adding Compressor...");
            Serial.flush();
            synth.getCompressor().setEnabled(true);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("5. Adding Resonator...");
            Serial.flush();
            synth.setResonatorEnabled(true);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("6. Adding Comb Filter...");
            Serial.flush();
            synth.setCombEnabled(true);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("7. Adding Granular Exciter...");
            Serial.flush();
            synth.setGranularEnabled(true);
            delay(2000);
            Serial.printf("   - Current CPU: %.1f%%\n", synth.getCPUPercent());

            Serial.println("Sequential stress test complete. Still alive!");
            Serial.flush();
            break;

        case 'S':
            Serial.println("Starting Stress Test (Gradual All systems GO)...");
            synth.setSynthMode(VoiceSynthMode::FM);
            synth.setFMAmount(5.0f);

            // Enable effects one by one with delay
            Serial.println("  Enabling basic effects...");
            synth.getEffects().setEnabled(true, true, true);
            delay(500);

            Serial.println("  Enabling master effects...");
            synth.getReverb().setEnabled(true);
            synth.getCompressor().setEnabled(true);
            delay(500);

            Serial.println("  Enabling spectral modules...");
            synth.setResonatorEnabled(true);
            synth.setCombEnabled(true);
            synth.setGranularEnabled(true);
            delay(500);

            for (int i = 0; i < NUM_VOICES; i++) {
                Serial.printf("  Triggering voice %d (Note %d)...\n", i, 48 + i * 7);
                synth.noteOn(48 + i * 7, 100);
                delay(1000); // 1 second between notes for stability monitoring
            }
            Serial.println("Stress Test running.");
            break;
        case 'V':
            Serial.println("PANIC: Stopping All Notes & Resetting Engine");
            synth.allNotesOff();
            synth.setResonatorEnabled(false);
            synth.setCombEnabled(false);
            synth.setGranularEnabled(false);
            synth.getEffects().setEnabled(false, false, false);
            synth.getReverb().setEnabled(false);
            synth.getCompressor().setEnabled(false);

            // Hard reset of all recursive modules
            synth.getResonator().reset();
            synth.getComb().reset();
            synth.getReverb().reset();
            synth.getEffects().delay.clear();
            synth.getEffects().chorus.clear();

            for (int i = 0; i < NUM_VOICES; i++) {
                synth.getVoice(i).forceOff();
            }
            Serial.println("Panic reset complete.");
            break;
        case 'm':
            Serial.printf("System Heap: %d bytes free\n", ESP.getFreeHeap());
            Serial.printf("Min Heap: %d bytes\n", ESP.getMinFreeHeap());
            break;
        // === GRANULAR / GLIDE ===
        case 'G':
        case 'g':
            if (c1 == 'l') {
                synth.setGlideTime(value);
                Serial.printf("Glide: %.0fms\n", value);
            } else if (c1 == 'x') {
                synth.setGranularEnabled(!synth.isGranularEnabled());
                Serial.printf("Granular: %s\n", synth.isGranularEnabled() ? "ON" : "OFF");
            } else if (c1 == 'm') {
                synth.setGranularMix(value / 100.0f);
                Serial.printf("Granular Mix: %.0f%%\n", value);
            } else if (c1 == 'd') {
                synth.getGranular().setDensity(value);
                Serial.printf("Granular Density: %.0f grains/sec\n", value);
            } else if (c1 == 't') {
                synth.getGranular().setDuration(value);
                Serial.printf("Granular Duration: %.0fms\n", value);
            } else if (c1 == 's') {
                synth.getGranular().setSource((GrainSource)((int)value % 5));
                Serial.printf("Granular Source: %d\n", (int)value % 5);
            }
            break;
        case 'v':
            synth.setMasterVolume(value / 100.0f);
            Serial.printf("Volume: %.0f%%\n", value);
            break;
        case '?':
            printHelp();
            break;
            
        default:
            Serial.println("Unknown. Type ? for help");
    }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Process Hardware Controls
    controls.update();
    encNav.update();
    encVal.update();

    // Handle Navigation Encoder
    int navDelta = encNav.getDelta();
    if (navDelta != 0) {
        if (display.getCurrentPage() == DisplayPage::MAIN) {
            if (navDelta > 0) display.nextPage();
            else display.prevPage();
        } else {
            if (navDelta > 0) display.nextItem();
            else display.prevItem();
        }
    }

    if (encNav.wasClicked()) {
        if (display.getCurrentPage() != DisplayPage::MAIN) {
            display.setPage(DisplayPage::MAIN);
        } else {
            display.nextPage();
        }
    }

    // Handle Value Encoder
    int valDelta = encVal.getDelta();
    if (valDelta != 0) {
        display.adjustValue(valDelta);
    }

    if (encVal.wasClicked()) {
        // Encoder 2 click could trigger something like "preview note"
        synth.noteOn(60, 100);
        delay(100);
        synth.noteOff(60);
    }

    // Process MIDI
    midi.process();

    // Deferred preset load: NVS flash is done here, not in the MIDI callback,
    // so MIDI clock ticks are not starved during a program-change message.
    if (presetPending_) {
        presetPending_ = false;
        PresetData p;
        if (presets.loadPreset(pendingPresetProgram_, p)) {
            applyPreset(p);
        }
    }
    
    // Process serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        processCommand(cmd);
    }

    // Heartbeat and auto-stats from loop() to avoid Serial deadlocks in audio task
    uint32_t currentBlock = synth.getBlockCount();
    if (currentBlock >= lastHeartbeatBlock + 375) { // ~1 second
        lastHeartbeatBlock = currentBlock;
        if (autoStats) {
            Serial.printf("[CPU: %.1f%%] ", synth.getCPUPercent());
        } else {
            Serial.print(".");
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
}
