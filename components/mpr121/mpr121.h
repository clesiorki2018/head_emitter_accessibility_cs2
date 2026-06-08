#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPR121_CHANNEL_COUNT 12
#define MPR121_DEFAULT_I2C_ADDRESS 0x5a
#define MPR121_DEFAULT_TOUCH_THRESHOLD 12
#define MPR121_DEFAULT_RELEASE_THRESHOLD 6

typedef struct {
    int sda_gpio;
    int scl_gpio;
    uint8_t i2c_address;
    uint8_t touch_threshold;
    uint8_t release_threshold;
    uint8_t channel_touch_thresholds[MPR121_CHANNEL_COUNT];
    uint8_t channel_release_thresholds[MPR121_CHANNEL_COUNT];
} mpr121_config_t;

typedef struct {
    mpr121_config_t config;
    void *bus_handle;
    void *device_handle;
} mpr121_t;

esp_err_t mpr121_init(mpr121_t *dev, const mpr121_config_t *config);
esp_err_t mpr121_read_touched(const mpr121_t *dev, uint16_t *touched_mask);

#ifdef __cplusplus
}
#endif
