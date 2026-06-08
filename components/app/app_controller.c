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
#define APP_ACCESSIBILITY_BOOT_SETTLE_MS 1500
#define APP_MAPPED_KEY_MAX_HOLD_MS 250
#define APP_RIGHT_CLICK_TOUCH_THRESHOLD 8
#define APP_RIGHT_CLICK_RELEASE_THRESHOLD 4

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

static bool touch_channel_is_set(uint16_t touched_mask, uint8_t channel)
{
    return (touched_mask & (uint16_t)(1u << channel)) != 0;
}

static esp_err_t update_realtime_right_click(uint16_t touched_mask, bool *pressed)
{
    if (pressed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool touched = touch_channel_is_set(touched_mask, ACCESSIBILITY_RIGHT_CLICK_CHANNEL);
    if (touched == *pressed) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Realtime right click E%d %s",
             ACCESSIBILITY_RIGHT_CLICK_CHANNEL,
             touched ? "PRESSED" : "RELEASED");

    esp_err_t err = send_mouse_button(APP_MOUSE_BUTTON_RIGHT, touched);
    if (err == ESP_OK) {
        *pressed = touched;
    }

    return err;
}

static esp_err_t release_realtime_right_click_if_pressed(bool *pressed)
{
    if (pressed == NULL || !*pressed) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Realtime right click released after input fault");
    esp_err_t err = send_mouse_button(APP_MOUSE_BUTTON_RIGHT, false);
    if (err == ESP_OK) {
        *pressed = false;
    }

    return err;
}

static esp_err_t dispatch_input_event(const input_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool pressed = input_event_is_pressed(event);

    ESP_LOGI(TAG, "Input event: action=%s type=%s",
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

static esp_err_t unlock_accessibility_boot_guard(accessibility_t *accessibility,
                                                 const accessibility_config_t *config)
{
    esp_err_t err = accessibility_init(accessibility, config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Accessibility module reinit failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Accessibility gestures enabled after boot guard");
    return ESP_OK;
}

static bool input_action_is_mapped_keyboard(input_action_t action)
{
    return action == INPUT_ACTION_KEY_W || action == INPUT_ACTION_KEY_Q;
}

static bool input_event_is_mapped_keyboard(const input_event_t *event)
{
    return event != NULL && input_action_is_mapped_keyboard(event->action);
}

static void update_mapped_keyboard_hold(const input_event_t *event,
                                        bool *pressed,
                                        input_action_t *pressed_action,
                                        uint32_t *release_at_ms)
{
    if (!input_event_is_mapped_keyboard(event)) {
        return;
    }

    if (input_event_is_pressed(event)) {
        *pressed = true;
        *pressed_action = event->action;
        *release_at_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) +
                         APP_MAPPED_KEY_MAX_HOLD_MS;
        return;
    }

    if (*pressed && *pressed_action == event->action) {
        *pressed = false;
    }
}

static void release_mapped_keyboard_if_due(bool *pressed,
                                           input_action_t *pressed_action,
                                           uint32_t release_at_ms)
{
    if (!*pressed) {
        return;
    }

    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if ((int32_t)(now_ms - release_at_ms) < 0) {
        return;
    }

    input_event_t release_event = {
        .action = *pressed_action,
        .type = INPUT_EVENT_RELEASED,
    };
    esp_err_t err = dispatch_input_event(&release_event);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Mapped keyboard failsafe release failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGW(TAG, "Mapped keyboard failsafe released %s",
             input_action_name(*pressed_action));
    *pressed = false;
}

static void release_active_inputs(input_mapper_t *mapper,
                                  accessibility_t *accessibility,
                                  bool accessibility_enabled)
{
    input_event_t events[APP_MAX_EVENTS_PER_POLL];
    size_t event_count = 0;

    esp_err_t err = input_mapper_release_all(mapper, events,
                                             APP_MAX_EVENTS_PER_POLL, &event_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to release mapped inputs: %s", esp_err_to_name(err));
    }

    for (size_t index = 0; index < event_count; ++index) {
        err = dispatch_input_event(&events[index]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Input release dispatch failed: %s", esp_err_to_name(err));
        }
    }

    if (!accessibility_enabled) {
        return;
    }

    err = accessibility_release_all(accessibility);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Accessibility release failed: %s", esp_err_to_name(err));
    }
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
    bool input_suspended_until_clear = false;
    bool realtime_right_click_pressed = false;
    bool mapped_keyboard_pressed = false;
    input_action_t mapped_keyboard_pressed_action = INPUT_ACTION_NONE;
    uint32_t mapped_keyboard_release_at_ms = 0;
    uint32_t accessibility_boot_unlock_at_ms = 0;

    mpr121_config_t touch_config = {
        .sda_gpio = APP_MPR121_SDA_GPIO,
        .scl_gpio = APP_MPR121_SCL_GPIO,
        .i2c_address = MPR121_DEFAULT_I2C_ADDRESS,
        .touch_threshold = MPR121_DEFAULT_TOUCH_THRESHOLD,
        .release_threshold = MPR121_DEFAULT_RELEASE_THRESHOLD,
    };
    touch_config.channel_touch_thresholds[ACCESSIBILITY_RIGHT_CLICK_CHANNEL] =
        APP_RIGHT_CLICK_TOUCH_THRESHOLD;
    touch_config.channel_release_thresholds[ACCESSIBILITY_RIGHT_CLICK_CHANNEL] =
        APP_RIGHT_CLICK_RELEASE_THRESHOLD;

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
    mapper_config.channel_actions[ACCESSIBILITY_RIGHT_CLICK_CHANNEL] = INPUT_ACTION_NONE;
    err = input_mapper_init(&mapper, &mapper_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Input mapper init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "MPR121 polling started");
    accessibility_boot_unlock_at_ms =
        (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) +
        APP_ACCESSIBILITY_BOOT_SETTLE_MS;

    while (true) {
        uint16_t touched_mask;
        input_event_t events[APP_MAX_EVENTS_PER_POLL];
        size_t event_count = 0;
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        err = mpr121_read_touched(&touch, &touched_mask);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "MPR121 read failed: %s", esp_err_to_name(err));
            esp_err_t release_err =
                release_realtime_right_click_if_pressed(&realtime_right_click_pressed);
            if (release_err != ESP_OK) {
                ESP_LOGW(TAG, "Realtime right click release failed: %s",
                         esp_err_to_name(release_err));
            }
            release_active_inputs(&mapper, &accessibility, accessibility_boot_unlocked);
            input_suspended_until_clear = true;
            mapped_keyboard_pressed = false;
            vTaskDelay(pdMS_TO_TICKS(APP_POLL_INTERVAL_MS));
            continue;
        }

        err = update_realtime_right_click(touched_mask, &realtime_right_click_pressed);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Realtime right click dispatch failed: %s", esp_err_to_name(err));
        }

        if (input_suspended_until_clear) {
            if (input_mapper_has_mapped_touch(&mapper, touched_mask)) {
                vTaskDelay(pdMS_TO_TICKS(APP_POLL_INTERVAL_MS));
                continue;
            }

            input_suspended_until_clear = false;
            ESP_LOGI(TAG, "MPR121 input resumed after all mapped channels released");
        }

        err = input_mapper_update(&mapper, touched_mask, events,
                                  APP_MAX_EVENTS_PER_POLL, &event_count);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Input mapping failed: %s", esp_err_to_name(err));
            release_active_inputs(&mapper, &accessibility, accessibility_boot_unlocked);
            input_suspended_until_clear = true;
            mapped_keyboard_pressed = false;
            vTaskDelay(pdMS_TO_TICKS(APP_POLL_INTERVAL_MS));
            continue;
        }

        if (event_count > 0) {
            ESP_LOGI(TAG, "Mapped touch: mask=0x%03x events=%u",
                     touched_mask & 0x0fff,
                     (unsigned)event_count);
        }

        if (!accessibility_boot_unlocked) {
            if (input_mapper_has_mapped_touch(&mapper, touched_mask)) {
                accessibility_boot_unlock_at_ms = now_ms + APP_ACCESSIBILITY_BOOT_SETTLE_MS;
            } else if ((int32_t)(now_ms - accessibility_boot_unlock_at_ms) >= 0) {
                err = unlock_accessibility_boot_guard(&accessibility, &accessibility_config);
                if (err != ESP_OK) {
                    vTaskDelete(NULL);
                }

                accessibility_boot_unlocked = true;
            }
        }

        for (size_t index = 0; index < event_count; ++index) {
            err = dispatch_input_event(&events[index]);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Input dispatch failed: %s", esp_err_to_name(err));
                continue;
            }

            update_mapped_keyboard_hold(&events[index],
                                        &mapped_keyboard_pressed,
                                        &mapped_keyboard_pressed_action,
                                        &mapped_keyboard_release_at_ms);
        }

        release_mapped_keyboard_if_due(&mapped_keyboard_pressed,
                                       &mapped_keyboard_pressed_action,
                                       mapped_keyboard_release_at_ms);

        if (accessibility_boot_unlocked) {
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
