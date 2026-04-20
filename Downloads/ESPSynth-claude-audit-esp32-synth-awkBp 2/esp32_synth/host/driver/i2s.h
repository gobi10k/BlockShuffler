#pragma once
// Stub for <driver/i2s.h> (ESP-IDF I2S driver).
// Only the symbols used by SynthEngine.cpp are declared here.
// i2s_write() is a no-op — the host main reads blockBuffer_ directly.

#include <stdint.h>
#include <stddef.h>

typedef int  i2s_port_t;
typedef int  i2s_mode_t;
typedef int  i2s_bits_per_sample_t;
typedef int  i2s_channel_fmt_t;
typedef int  i2s_comm_format_t;
typedef uint32_t TickType_t;

// Re-use esp_err_t from platform.h if already seen; define if not.
#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK   0
#define ESP_FAIL (-1)
#endif

#define I2S_NUM_0                     ((i2s_port_t)0)
#define I2S_PIN_NO_CHANGE             (-1)
#define I2S_MODE_MASTER               (1 << 0)
#define I2S_MODE_TX                   (1 << 1)
#define I2S_BITS_PER_SAMPLE_16BIT     ((i2s_bits_per_sample_t)16)
#define I2S_CHANNEL_FMT_RIGHT_LEFT    ((i2s_channel_fmt_t)3)
#define I2S_COMM_FORMAT_STAND_I2S     ((i2s_comm_format_t)1)
#define ESP_INTR_FLAG_LEVEL1          (1 << 1)

typedef struct {
    i2s_mode_t              mode;
    uint32_t                sample_rate;
    i2s_bits_per_sample_t   bits_per_sample;
    i2s_channel_fmt_t       channel_format;
    i2s_comm_format_t       communication_format;
    int                     intr_alloc_flags;
    int                     dma_buf_count;
    int                     dma_buf_len;
    bool                    use_apll;
    bool                    tx_desc_auto_clear;
    int                     fixed_mclk;
} i2s_config_t;

typedef struct {
    int mck_io_num;
    int bck_io_num;
    int ws_io_num;
    int data_out_num;
    int data_in_num;
} i2s_pin_config_t;

inline esp_err_t i2s_driver_install(i2s_port_t, const i2s_config_t*, int, void*) {
    return ESP_OK;
}
inline esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t*) {
    return ESP_OK;
}
inline esp_err_t i2s_write(i2s_port_t, const void*, size_t size,
                             size_t* bytes_written, TickType_t) {
    if (bytes_written) *bytes_written = size;
    return ESP_OK;
}
