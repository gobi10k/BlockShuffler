#pragma once
// platform.h — Arduino / ESP-IDF shim for the desktop (HOST_BUILD) build.
// Included by Config.h and MathUtils.h when HOST_BUILD is defined;
// never compiled on-device.

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <chrono>

// ── constrain / min / max ─────────────────────────────────────────────────────
// Arduino's constrain() is a macro; literal int constants (1, 4, 24 …) cause
// deduction conflicts when the first arg is uint8_t. Accept heterogeneous types
// and cast lo/hi to match val — identical to what the macro expansion does.
template<typename A, typename B, typename C>
inline A constrain(A val, B lo, C hi) {
    if (val < (A)lo) return (A)lo;
    if (val > (A)hi) return (A)hi;
    return val;
}

template<typename T> inline T min(T a, T b) { return a < b ? a : b; }
template<typename T> inline T max(T a, T b) { return a > b ? a : b; }

// ── Serial stub ───────────────────────────────────────────────────────────────
struct SerialClass {
    template<typename... Args>
    void printf(const char* fmt, Args... args) const {
        ::printf(fmt, args...);
        fflush(stdout);
    }
    void println(const char* s) const { puts(s); }
    void println() const { putchar('\n'); }
    void begin(unsigned long) const {}
    void setTimeout(unsigned long) const {}
    int available() const { return 0; }
    int read() const { return -1; }
};
// C++17 inline variable — one definition across all TUs.
inline SerialClass Serial;

// ── Timing ────────────────────────────────────────────────────────────────────
inline uint32_t micros() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count();
}
inline uint32_t millis() { return micros() / 1000u; }

// ── FreeRTOS stubs (single-threaded host, no-ops) ────────────────────────────
typedef int    portMUX_TYPE;
typedef void*  TaskHandle_t;

#define portMUX_INITIALIZER_UNLOCKED  0
#define taskENTER_CRITICAL(m)         ((void)(m))
#define taskEXIT_CRITICAL(m)          ((void)(m))
#define configMAX_PRIORITIES          24
#define pdMS_TO_TICKS(ms)             ((uint32_t)(ms))

inline void vTaskDelay(uint32_t) {}
inline void vTaskDelete(void*)   {}

// xTaskCreatePinnedToCore — no-op; host main drives the render loop directly.
#define xTaskCreatePinnedToCore(fn, name, stack, param, prio, handle, core) \
    do { if ((handle)) *(handle) = nullptr; } while (0)

// ── ESP-IDF error types ───────────────────────────────────────────────────────
typedef int  esp_err_t;
#define ESP_OK    0
#define ESP_FAIL  (-1)

// ── Compiler attributes (empty on desktop) ───────────────────────────────────
#define DRAM_ATTR
#define IRAM_ATTR
#define PROGMEM
