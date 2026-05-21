#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_PROTOCOL_MOUSE_MOVE_SIZE 5
#define INPUT_PROTOCOL_MOUSE_BUTTON_SIZE 3
#define INPUT_PROTOCOL_KEYBOARD_KEY_SIZE 3
#define INPUT_PROTOCOL_JOYSTICK_AXIS_SIZE 9
#define INPUT_PROTOCOL_JOYSTICK_BUTTON_SIZE 3

esp_err_t input_protocol_mouse_move(int16_t dx, int16_t dy, uint8_t *buf, size_t len);
esp_err_t input_protocol_mouse_button(uint8_t button, bool pressed, uint8_t *buf, size_t len);
esp_err_t input_protocol_keyboard_key(uint8_t keycode, bool pressed, uint8_t *buf, size_t len);
esp_err_t input_protocol_joystick_axis(int16_t x, int16_t y, int16_t z, int16_t rz,
                                       uint8_t *buf, size_t len);
esp_err_t input_protocol_joystick_button(uint8_t button, bool pressed, uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif
