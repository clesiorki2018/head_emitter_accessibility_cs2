#include "accessibility/accessibility.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "accessibility";

enum {
    ACCESSIBILITY_ALT_KEY_A_INDEX = 0,
    ACCESSIBILITY_ALT_KEY_D_INDEX = 1,
};

static bool channel_touched(uint16_t touched_mask, uint8_t channel)
{
    return (touched_mask & (uint16_t)(1u << channel)) != 0;
}

static uint32_t elapsed_ms(uint32_t now_ms, uint32_t since_ms)
{
    return now_ms - since_ms;
}

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static esp_err_t send_key(accessibility_t *accessibility, uint8_t keycode, bool pressed)
{
    if (accessibility->config.send_key == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return accessibility->config.send_key(keycode, pressed,
                                          accessibility->config.send_key_ctx);
}

static uint8_t alt_keycode(uint8_t index)
{
    return index == ACCESSIBILITY_ALT_KEY_A_INDEX
               ? ACCESSIBILITY_HID_KEY_A
               : ACCESSIBILITY_HID_KEY_D;
}

static esp_err_t release_alt_key_if_needed(accessibility_t *accessibility,
                                           uint8_t index,
                                           uint32_t now_ms)
{
    if (!accessibility->alt_key_pressed[index] ||
        !deadline_reached(now_ms, accessibility->alt_key_release_at_ms[index])) {
        return ESP_OK;
    }

    accessibility->alt_key_pressed[index] = false;
    return send_key(accessibility, alt_keycode(index), false);
}

static esp_err_t release_alt_keys(accessibility_t *accessibility)
{
    esp_err_t result = ESP_OK;

    for (uint8_t index = 0; index < 2; ++index) {
        if (!accessibility->alt_key_pressed[index]) {
            continue;
        }

        accessibility->alt_key_pressed[index] = false;
        esp_err_t err = send_key(accessibility, alt_keycode(index), false);
        if (result == ESP_OK) {
            result = err;
        }
    }

    return result;
}

static esp_err_t toggle_feature2(accessibility_t *accessibility, uint32_t now_ms)
{
    accessibility->feature2_enabled = !accessibility->feature2_enabled;
    accessibility->next_alt_key_index = ACCESSIBILITY_ALT_KEY_A_INDEX;
    accessibility->next_alt_press_at_ms = now_ms;
    ESP_LOGI(TAG, "Feature 2 %s", accessibility->feature2_enabled ? "enabled" : "disabled");

    if (!accessibility->feature2_enabled) {
        return release_alt_keys(accessibility);
    }

    return ESP_OK;
}

static esp_err_t update_feature1(accessibility_t *accessibility,
                                 bool click_touched,
                                 uint32_t now_ms)
{
    esp_err_t result = ESP_OK;

    if (accessibility->ctrl_pulse_active &&
        deadline_reached(now_ms, accessibility->ctrl_release_at_ms)) {
        accessibility->ctrl_pulse_active = false;
        result = send_key(accessibility, ACCESSIBILITY_HID_KEY_LEFT_CTRL, false);
    }

    if (accessibility->feature1_enabled && click_touched && !accessibility->click_touched &&
        !accessibility->ctrl_pulse_active) {
        esp_err_t err = send_key(accessibility, ACCESSIBILITY_HID_KEY_LEFT_CTRL, true);
        if (result == ESP_OK) {
            result = err;
        }

        if (err == ESP_OK) {
            accessibility->ctrl_pulse_active = true;
            accessibility->ctrl_release_at_ms = now_ms + ACCESSIBILITY_CTRL_HOLD_MS;
        }
    }

    return result;
}

static esp_err_t update_feature2(accessibility_t *accessibility, uint32_t now_ms)
{
    esp_err_t result = ESP_OK;

    for (uint8_t index = 0; index < 2; ++index) {
        esp_err_t err = release_alt_key_if_needed(accessibility, index, now_ms);
        if (result == ESP_OK) {
            result = err;
        }
    }

    if (!accessibility->feature2_enabled ||
        !deadline_reached(now_ms, accessibility->next_alt_press_at_ms)) {
        return result;
    }

    uint8_t index = accessibility->next_alt_key_index;
    esp_err_t err = send_key(accessibility, alt_keycode(index), true);
    if (result == ESP_OK) {
        result = err;
    }

    if (err == ESP_OK) {
        accessibility->alt_key_pressed[index] = true;
        accessibility->alt_key_release_at_ms[index] = now_ms + ACCESSIBILITY_ALT_KEY_HOLD_MS;
        accessibility->next_alt_key_index = index == ACCESSIBILITY_ALT_KEY_A_INDEX
                                                ? ACCESSIBILITY_ALT_KEY_D_INDEX
                                                : ACCESSIBILITY_ALT_KEY_A_INDEX;
        accessibility->next_alt_press_at_ms = now_ms + ACCESSIBILITY_ALT_KEY_PERIOD_MS;
    }

    return result;
}

static esp_err_t update_control_gesture(accessibility_t *accessibility,
                                        bool control_touched,
                                        uint32_t now_ms)
{
    if (control_touched && !accessibility->control_touched) {
        accessibility->control_pressed_at_ms = now_ms;
        accessibility->control_long_press_handled = false;
    }

    if (control_touched && !accessibility->control_long_press_handled &&
        elapsed_ms(now_ms, accessibility->control_pressed_at_ms) >= ACCESSIBILITY_LONG_PRESS_MS) {
        accessibility->control_long_press_handled = true;
        accessibility->tap_count = 0;
        return toggle_feature2(accessibility, now_ms);
    }

    if (!control_touched && accessibility->control_touched &&
        !accessibility->control_long_press_handled) {
        if (accessibility->tap_count == 0 ||
            elapsed_ms(now_ms, accessibility->last_tap_ms) <= ACCESSIBILITY_TAP_MAX_INTERVAL_MS) {
            ++accessibility->tap_count;
        } else {
            accessibility->tap_count = 1;
        }

        accessibility->last_tap_ms = now_ms;
        if (accessibility->tap_count >= ACCESSIBILITY_TRIPLE_TAP_COUNT) {
            accessibility->tap_count = 0;
            accessibility->feature1_enabled = !accessibility->feature1_enabled;
            ESP_LOGI(TAG, "Feature 1 %s",
                     accessibility->feature1_enabled ? "enabled" : "disabled");
        }
    }

    return ESP_OK;
}

void accessibility_default_config(accessibility_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (accessibility_config_t){0};
}

esp_err_t accessibility_init(accessibility_t *accessibility,
                             const accessibility_config_t *config)
{
    if (accessibility == NULL || config == NULL || config->send_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(accessibility, 0, sizeof(*accessibility));
    accessibility->config = *config;
    return ESP_OK;
}

esp_err_t accessibility_update(accessibility_t *accessibility,
                               uint16_t touched_mask,
                               uint32_t now_ms)
{
    if (accessibility == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool control_touched = channel_touched(touched_mask, ACCESSIBILITY_CONTROL_CHANNEL);
    bool click_touched = channel_touched(touched_mask, ACCESSIBILITY_CLICK_CHANNEL);

    esp_err_t result = update_control_gesture(accessibility, control_touched, now_ms);

    esp_err_t err = update_feature1(accessibility, click_touched, now_ms);
    if (result == ESP_OK) {
        result = err;
    }

    err = update_feature2(accessibility, now_ms);
    if (result == ESP_OK) {
        result = err;
    }

    accessibility->control_touched = control_touched;
    accessibility->click_touched = click_touched;

    return result;
}
