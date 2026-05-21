#include "app/app_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input/input_mapper.h"
#include "input_protocol/input_protocol.h"
#include "mpr121.h"
#include "transport_espnow/espnow_transport.h"

static const char *TAG = "app_controller";

#define APP_MPR121_SDA_GPIO 21
#define APP_MPR121_SCL_GPIO 22
#define APP_POLL_INTERVAL_MS 20
#define APP_MAX_EVENTS_PER_POLL INPUT_MAPPER_CHANNEL_COUNT

enum {
    APP_MOUSE_BUTTON_LEFT = 0,
    APP_MOUSE_BUTTON_RIGHT = 1,
    APP_MOUSE_BUTTON_MIDDLE = 2,
    APP_HID_KEY_W = 0x1a,
    APP_HID_KEY_Q = 0x14,
};

static esp_err_t send_payload(const uint8_t *payload, size_t len)
{
    esp_err_t err = espnow_transport_send(payload, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW send failed: %s", esp_err_to_name(err));
    }

    return err;
}

static esp_err_t send_mouse_button(uint8_t button, bool pressed)
{
    uint8_t payload[INPUT_PROTOCOL_JOYSTICK_AXIS_SIZE];
    esp_err_t err = input_protocol_mouse_button(button, pressed, payload, sizeof(payload));

    if (err != ESP_OK) {
        return err;
    }

    return send_payload(payload, INPUT_PROTOCOL_MOUSE_BUTTON_SIZE);
}

static esp_err_t send_keyboard_key(uint8_t keycode, bool pressed)
{
    uint8_t payload[INPUT_PROTOCOL_JOYSTICK_AXIS_SIZE];
    esp_err_t err = input_protocol_keyboard_key(keycode, pressed, payload, sizeof(payload));

    if (err != ESP_OK) {
        return err;
    }

    return send_payload(payload, INPUT_PROTOCOL_KEYBOARD_KEY_SIZE);
}

static esp_err_t dispatch_input_event(const input_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool pressed = input_event_is_pressed(event);

    switch (event->action) {
    case INPUT_ACTION_MOUSE_LEFT:
        return send_mouse_button(APP_MOUSE_BUTTON_LEFT, pressed);
    case INPUT_ACTION_MOUSE_RIGHT:
        return send_mouse_button(APP_MOUSE_BUTTON_RIGHT, pressed);
    case INPUT_ACTION_MOUSE_MIDDLE:
        return send_mouse_button(APP_MOUSE_BUTTON_MIDDLE, pressed);
    case INPUT_ACTION_KEY_W:
        return send_keyboard_key(APP_HID_KEY_W, pressed);
    case INPUT_ACTION_KEY_Q:
        return send_keyboard_key(APP_HID_KEY_Q, pressed);
    case INPUT_ACTION_NONE:
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static void app_input_task(void *arg)
{
    (void)arg;

    mpr121_t touch;
    input_mapper_t mapper;
    input_mapper_config_t mapper_config;

    mpr121_config_t touch_config = {
        .sda_gpio = APP_MPR121_SDA_GPIO,
        .scl_gpio = APP_MPR121_SCL_GPIO,
        .i2c_address = MPR121_DEFAULT_I2C_ADDRESS,
        .touch_threshold = MPR121_DEFAULT_TOUCH_THRESHOLD,
        .release_threshold = MPR121_DEFAULT_RELEASE_THRESHOLD,
    };

    esp_err_t err = mpr121_init(&touch, &touch_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPR121 init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    input_mapper_default_config(&mapper_config);
    err = input_mapper_init(&mapper, &mapper_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Input mapper init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "MPR121 polling started");

    while (true) {
        uint16_t touched_mask;
        input_event_t events[APP_MAX_EVENTS_PER_POLL];
        size_t event_count = 0;

        err = mpr121_read_touched(&touch, &touched_mask);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "MPR121 read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(APP_POLL_INTERVAL_MS));
            continue;
        }

        err = input_mapper_update(&mapper, touched_mask, events,
                                  APP_MAX_EVENTS_PER_POLL, &event_count);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Input mapping failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(APP_POLL_INTERVAL_MS));
            continue;
        }

        for (size_t index = 0; index < event_count; ++index) {
            err = dispatch_input_event(&events[index]);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Input dispatch failed: %s", esp_err_to_name(err));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_POLL_INTERVAL_MS));
    }
}

esp_err_t app_controller_init(void)
{
    esp_err_t err = espnow_transport_init();
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t result = xTaskCreate(app_input_task, "mpr121_input", 4096, NULL, 5, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MPR121 input task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Input controller ready");
    return ESP_OK;
}
