#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "accessibility/accessibility_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*accessibility_key_sender_t)(uint8_t keycode, bool pressed, void *ctx);

typedef struct {
    accessibility_key_sender_t send_key;
    void *send_key_ctx;
} accessibility_config_t;

typedef struct {
    accessibility_config_t config;
    bool feature1_enabled;
    bool feature2_enabled;
    bool control_touched;
    bool control_candidate_touched;
    bool control_hold_tracking;
    bool right_click_touched;
    bool right_click_candidate_touched;
    bool control_long_press_handled;
    bool ctrl_pulse_active;
    bool feature2_w_pressed;
    bool alt_key_pressed[2];
    uint8_t control_candidate_count;
    uint8_t right_click_candidate_count;
    uint8_t tap_count;
    uint8_t next_alt_key_index;
    uint32_t control_pressed_at_ms;
    uint32_t control_released_at_ms;
    uint32_t last_tap_ms;
    uint32_t ctrl_release_at_ms;
    uint32_t next_w_keepalive_at_ms;
    uint32_t next_alt_press_at_ms;
    uint32_t alt_key_release_at_ms[2];
} accessibility_t;

void accessibility_default_config(accessibility_config_t *config);
esp_err_t accessibility_init(accessibility_t *accessibility,
                             const accessibility_config_t *config);
esp_err_t accessibility_update(accessibility_t *accessibility,
                               uint16_t touched_mask,
                               uint32_t now_ms);
esp_err_t accessibility_release_all(accessibility_t *accessibility);

#ifdef __cplusplus
}
#endif
