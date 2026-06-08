/*
 * Copyright 2026 clesiorki2018
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mpr121.h"
#include <stddef.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPR121_REG_TOUCH_STATUS_L 0x00
#define MPR121_REG_TOUCH_THRESHOLD_BASE 0x41
#define MPR121_REG_RELEASE_THRESHOLD_BASE 0x42
#define MPR121_REG_DEBOUNCE 0x5b
#define MPR121_REG_CONFIG_1 0x5c
#define MPR121_REG_CONFIG_2 0x5d
#define MPR121_REG_ELECTRODE_CONFIG 0x5e
#define MPR121_REG_SOFT_RESET 0x80

#define MPR121_TOUCH_STATUS_MASK 0x0fff
#define MPR121_I2C_TIMEOUT_MS 100

static esp_err_t mpr121_write_u8(const mpr121_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t tx_buf[] = {reg, value};

    return i2c_master_transmit((i2c_master_dev_handle_t)dev->device_handle,
                               tx_buf, sizeof(tx_buf), MPR121_I2C_TIMEOUT_MS);
}

static esp_err_t mpr121_read(const mpr121_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive((i2c_master_dev_handle_t)dev->device_handle,
                                       &reg, sizeof(reg), data, len,
                                       MPR121_I2C_TIMEOUT_MS);
}

static esp_err_t mpr121_set_thresholds(const mpr121_t *dev)
{
    for (uint8_t channel = 0; channel < MPR121_CHANNEL_COUNT; ++channel) {
        uint8_t touch_reg = MPR121_REG_TOUCH_THRESHOLD_BASE + (channel * 2);
        uint8_t release_reg = MPR121_REG_RELEASE_THRESHOLD_BASE + (channel * 2);
        uint8_t touch_threshold = dev->config.channel_touch_thresholds[channel] != 0 ?
                                  dev->config.channel_touch_thresholds[channel] :
                                  dev->config.touch_threshold;
        uint8_t release_threshold = dev->config.channel_release_thresholds[channel] != 0 ?
                                    dev->config.channel_release_thresholds[channel] :
                                    dev->config.release_threshold;

        esp_err_t err = mpr121_write_u8(dev, touch_reg, touch_threshold);
        if (err != ESP_OK) {
            return err;
        }

        err = mpr121_write_u8(dev, release_reg, release_threshold);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t mpr121_init(mpr121_t *dev, const mpr121_config_t *config)
{
    if (dev == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->sda_gpio < 0 || config->scl_gpio < 0 || config->i2c_address == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *dev = (mpr121_t){.config = *config};

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_address,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t device_handle;
    err = i2c_master_bus_add_device(bus_handle, &device_config, &device_handle);
    if (err != ESP_OK) {
        i2c_del_master_bus(bus_handle);
        return err;
    }

    dev->bus_handle = bus_handle;
    dev->device_handle = device_handle;

    err = mpr121_write_u8(dev, MPR121_REG_SOFT_RESET, 0x63);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    err = mpr121_write_u8(dev, MPR121_REG_ELECTRODE_CONFIG, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    err = mpr121_set_thresholds(dev);
    if (err != ESP_OK) {
        return err;
    }

    /* Conservative defaults from common MPR121 reference setups. */
    err = mpr121_write_u8(dev, MPR121_REG_DEBOUNCE, 0x11);
    if (err != ESP_OK) {
        return err;
    }

    err = mpr121_write_u8(dev, MPR121_REG_CONFIG_1, 0x10);
    if (err != ESP_OK) {
        return err;
    }

    err = mpr121_write_u8(dev, MPR121_REG_CONFIG_2, 0x20);
    if (err != ESP_OK) {
        return err;
    }

    return mpr121_write_u8(dev, MPR121_REG_ELECTRODE_CONFIG, MPR121_CHANNEL_COUNT);
}

esp_err_t mpr121_read_touched(const mpr121_t *dev, uint16_t *touched_mask)
{
    if (dev == NULL || dev->device_handle == NULL || touched_mask == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2];
    esp_err_t err = mpr121_read(dev, MPR121_REG_TOUCH_STATUS_L, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    *touched_mask = ((uint16_t)data[0] | ((uint16_t)data[1] << 8)) & MPR121_TOUCH_STATUS_MASK;
    return ESP_OK;
}
