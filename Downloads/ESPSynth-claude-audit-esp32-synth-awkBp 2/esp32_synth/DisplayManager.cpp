#include "DisplayManager.h"
#include "SynthEngine.h"
#include <Wire.h>

extern const char* WAVEFORM_NAMES[];
extern const char* FILTER_MODE_NAMES[];

DisplayManager::DisplayManager() :
    display_(U8G2_R0, U8X8_PIN_NONE),
    engine_(nullptr),
    displayTaskHandle_(nullptr),
    refreshDelayMs_(50),
    running_(false),
    currentPage_(DisplayPage::MAIN),
    selectedItem_(0),
    popupName_(""),
    popupValue_(0.0f),
    popupStartTime_(0)
{
}

bool DisplayManager::init(SynthEngine* engine) {
    engine_ = engine;
    
    Serial.printf("[Display] Initializing on SDA:%d, SCL:%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);
    delay(100); // Give OLED time to power up
    
    if (!display_.begin()) {
        Serial.println("[Display] SH1106 Init failed");
        return false;
    }
    
    display_.setFont(u8g2_font_5x7_tf);
    display_.setContrast(200);
    
    Serial.println("[Display] Ready");
    return true;
}

void DisplayManager::start() {
    if (running_) return;
    running_ = true;
    
    xTaskCreatePinnedToCore(
        displayTaskWrapper,
        "DisplayTask",
        8192,  // Increased stack size
        this,
        1,
        &displayTaskHandle_,
        0
    );
}

void DisplayManager::stop() {
    running_ = false;
    if (displayTaskHandle_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(displayTaskHandle_);
        displayTaskHandle_ = nullptr;
    }
}

void DisplayManager::update() {
    display_.firstPage();
    do {
        drawUI();
    } while (display_.nextPage());
}

void DisplayManager::setRefreshRate(uint8_t fps) {
    fps = constrain(fps, 1, 60);
    refreshDelayMs_ = 1000 / fps;
}

void DisplayManager::nextPage() {
    int page = (int)currentPage_ + 1;
    if (page >= (int)DisplayPage::NUM_PAGES) page = 0;
    currentPage_ = (DisplayPage)page;
    selectedItem_ = 0;
}

void DisplayManager::prevPage() {
    int page = (int)currentPage_ - 1;
    if (page < 0) page = (int)DisplayPage::NUM_PAGES - 1;
    currentPage_ = (DisplayPage)page;
    selectedItem_ = 0;
}

void DisplayManager::nextItem() {
    selectedItem_++;
    int maxItems = 4;
    if (currentPage_ == DisplayPage::MAIN) maxItems = 0;
    else if (currentPage_ == DisplayPage::FILTER) maxItems = 6;

    if (selectedItem_ >= maxItems) selectedItem_ = 0;
}

void DisplayManager::prevItem() {
    selectedItem_--;
    int maxItems = 4;
    if (currentPage_ == DisplayPage::MAIN) maxItems = 0;
    else if (currentPage_ == DisplayPage::FILTER) maxItems = 6;

    if (selectedItem_ < 0) selectedItem_ = maxItems - 1;
    if (selectedItem_ < 0) selectedItem_ = 0;
}

void DisplayManager::adjustValue(int delta) {
    if (!engine_) return;

    switch (currentPage_) {
        case DisplayPage::OSCILLATORS:
            switch (selectedItem_) {
                case 0: {
                    int wf = (int)engine_->getOscWaveform(0) + delta;
                    while (wf < 0) wf += 7;
                    engine_->setOscWaveform(0, (Waveform)(wf % 7));
                    showPopup("OSC1", (float)((wf % 7)));
                    break;
                }
                case 1: {
                    int wf = (int)engine_->getOscWaveform(1) + delta;
                    while (wf < 0) wf += 7;
                    engine_->setOscWaveform(1, (Waveform)(wf % 7));
                    showPopup("OSC2", (float)((wf % 7)));
                    break;
                }
                case 2: {
                    float newMix = constrain(engine_->getOscMix() + delta * 0.05f, 0.0f, 1.0f);
                    engine_->setOscMix(newMix);
                    showPopup("OSC Mix", newMix);
                    break;
                }
                case 3: {
                    float newDetune = constrain(engine_->getOscDetune(1) + delta * 0.5f, -50.0f, 50.0f);
                    engine_->setOscDetune(1, newDetune);
                    showPopup("Detune", newDetune);
                    break;
                }
            }
            break;

        case DisplayPage::FILTER:
            switch (selectedItem_) {
                case 0: {
                    int ft = (int)engine_->getFilterType() + delta;
                    while (ft < 0) ft += 2;
                    engine_->setFilterType((VoiceFilterType)(ft % 2));
                    showPopup("Filter", (float)(ft % 2));
                    break;
                }
                case 1: {
                    int fm = (int)engine_->getFilterMode() + delta;
                    while (fm < 0) fm += 4;
                    engine_->setFilterMode((FilterMode)(fm % 4));
                    showPopup("Mode", (float)(fm % 4));
                    break;
                }
                case 2: {
                    float newCutoff = constrain(engine_->getFilterCutoff() * (1.0f + delta * 0.05f), 20.0f, 20000.0f);
                    engine_->setFilterCutoff(newCutoff);
                    showPopup("Cutoff", newCutoff / 1000.0f);
                    break;
                }
                case 3: {
                    float newReso = constrain(engine_->getFilterResonance() + delta * 0.05f, 0.0f, 1.0f);
                    engine_->setFilterResonance(newReso);
                    showPopup("Reso", newReso);
                    break;
                }
                case 4: {
                    float newKT = constrain(engine_->getFilterKeyTracking() + delta * 0.1f, 0.0f, 1.0f);
                    engine_->setFilterKeyTracking(newKT);
                    showPopup("KeyTrack", newKT);
                    break;
                }
                case 5: {
                    float newVel = constrain(engine_->getFilterEnvVelocity() + delta * 0.1f, 0.0f, 1.0f);
                    engine_->setFilterEnvVelocity(newVel);
                    showPopup("Vel", newVel);
                    break;
                }
            }
            break;

        case DisplayPage::EFFECTS:
            {
                EffectsChain& fx = engine_->getEffects();
                switch (selectedItem_) {
                    case 0: fx.setEnabled(!fx.isSatEnabled(), fx.isChorusEnabled(), fx.isDelayEnabled()); break;
                    case 1: fx.setEnabled(fx.isSatEnabled(), !fx.isChorusEnabled(), fx.isDelayEnabled()); break;
                    case 2: fx.setEnabled(fx.isSatEnabled(), fx.isChorusEnabled(), !fx.isDelayEnabled()); break;
                    case 3: engine_->getReverb().setEnabled(!engine_->getReverb().isEnabled()); break;
                }
            }
            break;

        default: break;
    }
}

void DisplayManager::drawUI() {
    if (!engine_) return;

    // Check for popup first
    if (millis() - popupStartTime_ < POPUP_DURATION_MS) {
        display_.clearDisplay();
        drawPopup();
        return;
    }

    switch (currentPage_) {
        case DisplayPage::MAIN: drawMainPage(); break;
        case DisplayPage::OSCILLATORS: drawOscPage(); break;
        case DisplayPage::FILTER: drawFilterPage(); break;
        case DisplayPage::EFFECTS: drawEffectsPage(); break;
        case DisplayPage::SD_BROWSER: drawSDPage(); break;
        default: drawMainPage();
    }
}

void DisplayManager::drawMainPage() {
    char buf[32];
    int y = 7;
    
    display_.drawStr(0, y, "MAIN");
    snprintf(buf, sizeof(buf), "V:%d/%d", engine_->getActiveVoiceCount(), NUM_VOICES);
    display_.drawStr(60, y, buf);

    snprintf(buf, sizeof(buf), "%.1f%%", engine_->getCPUPercent());
    display_.drawStr(100, y, buf);
    
    y += 2;
    display_.drawLine(0, y, 127, y);
    y += 12;
    
    // Visual indicators
    float ampEnv = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (engine_->getVoice(i).isActive()) {
            ampEnv = max(ampEnv, engine_->getVoice(i).getLevel());
        }
    }

    display_.drawStr(0, y, "LEVEL:");
    display_.drawFrame(40, y-7, 80, 8);
    display_.drawBox(41, y-6, (int)(ampEnv * 78), 6);
    y += 14;

    display_.drawStr(0, y, "LFO:");
    float lfo = (engine_->getLFO(0).getRawValue() + 1.0f) * 0.5f;
    display_.drawFrame(40, y-7, 80, 8);
    display_.drawBox(41, y-6, (int)(lfo * 78), 6);
    y += 14;

    // Effects footer
    EffectsChain& fx = engine_->getEffects();
    display_.drawStr(0, 63, fx.isSatEnabled() ? "[S]" : " S ");
    display_.drawStr(25, 63, fx.isChorusEnabled() ? "[C]" : " C ");
    display_.drawStr(50, 63, fx.isDelayEnabled() ? "[D]" : " D ");
    display_.drawStr(75, 63, engine_->getReverb().isEnabled() ? "[R]" : " R ");
    display_.drawStr(100, 63, engine_->getCompressor().isEnabled() ? "[K]" : " K ");
}

void DisplayManager::drawOscPage() {
    char buf[32];
    display_.drawStr(0, 7, "OSCILLATORS");
    display_.drawLine(0, 9, 127, 9);

    Waveform wf1 = engine_->getOscWaveform(0);
    Waveform wf2 = engine_->getOscWaveform(1);

    display_.drawStr(0, 22, selectedItem_ == 0 ? "> OSC1:" : "  OSC1:");
    display_.drawStr(45, 22, WAVEFORM_NAMES[(int)wf1]);

    display_.drawStr(0, 34, selectedItem_ == 1 ? "> OSC2:" : "  OSC2:");
    display_.drawStr(45, 34, WAVEFORM_NAMES[(int)wf2]);

    snprintf(buf, sizeof(buf), "%s MIX: %.0f%%", selectedItem_ == 2 ? ">" : " ", engine_->getOscMix() * 100.0f);
    display_.drawStr(0, 46, buf);

    snprintf(buf, sizeof(buf), "%s DETUNE: %.1f", selectedItem_ == 3 ? ">" : " ", engine_->getOscDetune(1));
    display_.drawStr(0, 58, buf);
}

void DisplayManager::drawFilterPage() {
    char buf[32];
    display_.drawStr(0, 7, "FILTER");
    display_.drawLine(0, 9, 127, 9);

    const char* typeName = (engine_->getFilterType() == VoiceFilterType::SVF) ? "SVF" : "LADDER";
    snprintf(buf, sizeof(buf), "%s TYP:%s", selectedItem_ == 0 ? ">" : " ", typeName);
    display_.drawStr(0, 22, buf);

    snprintf(buf, sizeof(buf), "%s MOD:%s", selectedItem_ == 1 ? ">" : " ", FILTER_MODE_NAMES[(int)engine_->getFilterMode()]);
    display_.drawStr(64, 22, buf);

    float cutoff = engine_->getFilterCutoff();
    snprintf(buf, sizeof(buf), "%s CUTOFF: %.0f Hz", selectedItem_ == 2 ? ">" : " ", cutoff);
    display_.drawStr(0, 34, buf);

    snprintf(buf, sizeof(buf), "%s RESO: %.0f%%", selectedItem_ == 3 ? ">" : " ", engine_->getFilterResonance() * 100.0f);
    display_.drawStr(0, 46, buf);

    snprintf(buf, sizeof(buf), "%s KBD: %.0f%%", selectedItem_ == 4 ? ">" : " ", engine_->getFilterKeyTracking() * 100.0f);
    display_.drawStr(0, 58, buf);

    snprintf(buf, sizeof(buf), "%s VEL: %.0f%%", selectedItem_ == 5 ? ">" : " ", engine_->getFilterEnvVelocity() * 100.0f);
    display_.drawStr(64, 58, buf);
}

void DisplayManager::drawEffectsPage() {
    display_.drawStr(0, 7, "EFFECTS");
    display_.drawLine(0, 9, 127, 9);
    
    EffectsChain& fx = engine_->getEffects();

    display_.drawStr(0, 22, selectedItem_ == 0 ? "> SAT:" : "  SAT:");
    display_.drawStr(50, 22, fx.isSatEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 34, selectedItem_ == 1 ? "> CHORUS:" : "  CHORUS:");
    display_.drawStr(60, 34, fx.isChorusEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 46, selectedItem_ == 2 ? "> DELAY:" : "  DELAY:");
    display_.drawStr(55, 46, fx.isDelayEnabled() ? "ON" : "OFF");

    display_.drawStr(0, 58, selectedItem_ == 3 ? "> REVERB:" : "  REVERB:");
    display_.drawStr(60, 58, engine_->getReverb().isEnabled() ? "ON" : "OFF");
}

void DisplayManager::drawSDPage() {
    display_.drawStr(0, 7, "SD CARD STATUS");
    display_.drawLine(0, 9, 127, 9);

    display_.drawStr(0, 22, "FILE SYSTEM: FAT32");
    display_.drawStr(0, 34, "STATUS: READY");

    char buf[32];
    snprintf(buf, sizeof(buf), "PRESETS: %d FILES", 12); // Dummy count
    display_.drawStr(0, 46, buf);

    display_.drawStr(0, 58, "ENCODER 2: BROWSE");
}

void DisplayManager::displayTaskWrapper(void* param) {
    DisplayManager* mgr = static_cast<DisplayManager*>(param);
    while (mgr->running_) {
        mgr->update();
        vTaskDelay(pdMS_TO_TICKS(mgr->refreshDelayMs_));
    }
    vTaskDelete(NULL);
}

void DisplayManager::showPopup(const char* name, float value) {
    strncpy(popupName_, name, 15);
    popupName_[15] = '\0';
    popupValue_ = value;
    popupStartTime_ = millis();
}

void DisplayManager::drawPopup() {
    if (millis() - popupStartTime_ > POPUP_DURATION_MS) return;

    display_.setFont(u8g2_font_ncenB14_tr);
    display_.setCursor(10, 30);
    display_.print(popupName_);

    display_.setFont(u8g2_font_ncenB14_tr);
    display_.setCursor(10, 50);
    display_.print(popupValue_);

    // Bar graph
    uint8_t barWidth = map(constrain(popupValue_, 0.0f, 1.0f) * 100, 0, 100, 0, 108);
    display_.drawBox(10, 56, barWidth, 6);
}
