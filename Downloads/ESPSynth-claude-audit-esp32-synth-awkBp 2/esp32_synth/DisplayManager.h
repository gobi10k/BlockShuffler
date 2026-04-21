#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Config.h"
#include <U8g2lib.h>

class SynthEngine;

enum class DisplayPage : uint8_t {
    MAIN = 0,
    OSCILLATORS,
    FILTER,
    EFFECTS,
    SD_BROWSER,
    NUM_PAGES
};

class DisplayManager {
public:
    DisplayManager();
    
    bool init(SynthEngine* engine);
    void start();
    void stop();
    void update();
    void setRefreshRate(uint8_t fps);

    void setPage(DisplayPage page) { currentPage_ = page; }
    void nextPage();
    void prevPage();

    void nextItem();
    void prevItem();
    void adjustValue(int delta);

    DisplayPage getCurrentPage() const { return currentPage_; }

    // Popup feedback
    void showPopup(const char* name, float value);

private:
    void drawUI();
    void drawMainPage();
    void drawOscPage();
    void drawFilterPage();
    void drawEffectsPage();
    void drawSDPage();
    void drawPopup();

    static void displayTaskWrapper(void* param);

    // Popup state
    char popupName_[16];
    float popupValue_;
    uint32_t popupStartTime_;
    static const uint32_t POPUP_DURATION_MS = 2000;
    
    U8G2_SH1106_128X64_NONAME_1_HW_I2C display_;
    SynthEngine* engine_;
    TaskHandle_t displayTaskHandle_;
    uint16_t refreshDelayMs_;
    volatile bool running_;
    DisplayPage currentPage_;
    int8_t selectedItem_;
};

#endif
