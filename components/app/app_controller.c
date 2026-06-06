#include "app/app_controller.h"
#include <inttypes.h>
#include "accessibility/accessibility.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input/input_mapper.h"
#include "input_protocol/input_protocol.h"
#include "mpr121.h"
#include "security/secure_envelope.h"
#include "sequence_store/sequence_store.h"
#include "transport_espnow/espnow_transport.h"

static const char *TAG = "app_controller";

#define APP_MPR121_SDA_GPIO 21
#define APP_MPR121_SCL_GPIO 22
#define APP_POLL_INTERVAL_MS 20
#define APP_MAX_EVENTS_PER_POLL INPUT_MAPPER_CHANNEL_COUNT
#define APP_ACCESSIBILITY_BOOT_REQUIRED_CLICKS 3

enum {
    APP_MOUSE_BUTTON_LEFT = 0,
    APP_MOUSE_BUTTON_RIGHT = 1,
    APP_MOUSE_BUTTON_MIDDLE = 2,
    APP_HID_KEY_W = 0x1a,
    APP_HID_KEY_Q = 0x14,
};

static const char *input_action_name(input_action_t action)
{
    switch (action) {
    case INPUT_ACTION_MOUSE_LEFT:
        return "MOUSE_LEFT";
    case INPUT_ACTION_MOUSE_RIGHT:
        return "MOUSE_RIGHT";
    case INPUT_ACTION_MOUSE_MIDDLE:
        return "MOUSE_MIDDLE";
    case INPUT_ACTION_KEY_W:
        return "KEY_W";
    case INPUT_ACTION_KEY_Q:
        return "KEY_Q";
    case INPUT_ACTION_NONE:
        return "NONE";
    default:
        return "UNKNOWN";
    }
}

static const char *input_event_type_name(input_event_type_t type)
{
    switch (type) {
    case INPUT_EVENT_PRESSED:
        return "PRESSED";
    case INPUT_EVENT_RELEASED:
        return "RELEASED";
    default:
        return "UNKNOWN";
    }
}

static const char *command_opcode_name(uint8_t opcode)
{
    switch (opcode) {
    case 0x01:
        return "MOUSE_MOVE";
    case 0x02:
        return "MOUSE_BUTTON";
    case 0x03:
        return "KEYBOARD_KEY";
    case 0x04:
        return "JOYSTICK_AXIS";
    case 0x05:
        return "JOYSTICK_BUTTON";
    default:
        return "UNKNOWN";
    }
}

static esp_err_t send_payload(const uint8_t *payload, size_t len)
{
    uint8_t packet[SECURE_ENVELOPE_MAX_SIZE];
    size_t packet_len = 0;
    uint32_t sequence = 0;

    esp_err_t err = sequence_store_next(&sequence);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sequence store failed: %s", esp_err_to_name(err));
        return err;
    }

    err = secure_envelope_build(sequence, payload, len, packet, sizeof(packet), &packet_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Secure envelope build failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t opcode = payload[0];
    uint8_t arg0 = len > 1 ? payload[1] : 0;
    uint8_t arg1 = len > 2 ? payload[2] : 0;
    ESP_LOGD(TAG,
             "Sending command: sequence=%" PRIu32 " opcode=0x%02x(%s) len=%u arg0=%u arg1=%u envelope_len=%u",
             sequence,
             opcode,
             command_opcode_name(opcode),
             (unsigned)len,
             arg0,
             arg1,
             (unsigned)packet_len);

    err = espnow_transport_send(packet, packet_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW send failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "ESP-NOW send queued: sequence=%" PRIu32, sequence);
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

static esp_err_t accessibility_send_keyboard_key(uint8_t keycode, bool pressed, void *ctx)
{
    (void)ctx;
    return send_keyboard_key(keycode, pressed);
}

static esp_err_t dispatch_input_event(const input_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool pressed = input_event_is_pressed(event);

    ESP_LOGD(TAG, "Dispatch input event: action=%s type=%s",
             input_action_name(event->action), input_event_type_name(event->type));

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

static bool input_event_is_left_click(const input_event_t *event)
{
    return event != NULL && event->action == INPUT_ACTION_MOUSE_LEFT;
}

static void app_input_task(void *arg)
{
    (void)arg;

    mpr121_t touch;
    accessibility_t accessibility;
    accessibility_config_t accessibility_config;
    input_mapper_t mapper;
    input_mapper_config_t mapper_config;
    bool accessibility_boot_unlocked = false;
    bool left_click_pressed = false;
    uint8_t boot_click_count = 0;

    mpr121_config_t touch_config = {
        .sda_gpio = APP_MPR121_SDA_GPIO,
        .scl_gpio = APP_MPR121_SCL_GPIO,
        .i2c_address = MPR121_DEFAULT_I2C_ADDRESS,
        .touch_threshold = MPR121_DEFAULT_TOUCH_THRESHOLD,
        .release_threshold = MPR121_DEFAULT_RELEASE_THRESHOLD,
    };

    esp_err_t err = mpr121_init(&touch, &touch_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "MPR121 unavailable at I2C address 0x%02x on SDA=%d SCL=%d: %s; touch input disabled",
                 touch_config.i2c_address,
                 touch_config.sda_gpio,
                 touch_config.scl_gpio,
                 esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    accessibility_default_config(&accessibility_config);
    accessibility_config.send_key = accessibility_send_keyboard_key;
    err = accessibility_init(&accessibility, &accessibility_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Accessibility module init failed: %s", esp_err_to_name(err));
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
                continue;
            }

            if (!accessibility_boot_unlocked && input_event_is_left_click(&events[index])) {
                if (input_event_is_pressed(&events[index])) {
                    left_click_pressed = true;
                } else if (left_click_pressed) {
                    left_click_pressed = false;
                    ++boot_click_count;
                    ESP_LOGI(TAG, "Accessibility boot guard click %u/%u",
                             boot_click_count, APP_ACCESSIBILITY_BOOT_REQUIRED_CLICKS);

                    if (boot_click_count >= APP_ACCESSIBILITY_BOOT_REQUIRED_CLICKS) {
                        err = accessibility_init(&accessibility, &accessibility_config);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "Accessibility module reinit failed: %s",
                                     esp_err_to_name(err));
                            vTaskDelete(NULL);
                        }

                        accessibility_boot_unlocked = true;
                        ESP_LOGI(TAG, "Accessibility gestures enabled after boot guard");
                    }
                }
            }
        }

        if (accessibility_boot_unlocked) {
            uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            err = accessibility_update(&accessibility, touched_mask, now_ms);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Accessibility update failed: %s", esp_err_to_name(err));
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

    err = sequence_store_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sequence store init failed: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t result = xTaskCreate(app_input_task, "mpr121_input", 4096, NULL, 5, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MPR121 input task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "App controller ready");
    return ESP_OK;
}
