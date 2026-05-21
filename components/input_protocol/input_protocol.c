#include "input_protocol/input_protocol.h"

enum {
    INPUT_PROTOCOL_OPCODE_MOUSE_MOVE = 0x01,
    INPUT_PROTOCOL_OPCODE_MOUSE_BUTTON = 0x02,
    INPUT_PROTOCOL_OPCODE_KEYBOARD_KEY = 0x03,
    INPUT_PROTOCOL_OPCODE_JOYSTICK_AXIS = 0x04,
    INPUT_PROTOCOL_OPCODE_JOYSTICK_BUTTON = 0x05,
};

static esp_err_t require_buffer(uint8_t *buf, size_t len, size_t required_len)
{
    if (buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (len < required_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static void write_i16_le(uint8_t *buf, int16_t value)
{
    uint16_t encoded = (uint16_t)value;

    buf[0] = (uint8_t)(encoded & 0xffu);
    buf[1] = (uint8_t)(encoded >> 8);
}

esp_err_t input_protocol_mouse_move(int16_t dx, int16_t dy, uint8_t *buf, size_t len)
{
    esp_err_t err = require_buffer(buf, len, INPUT_PROTOCOL_MOUSE_MOVE_SIZE);
    if (err != ESP_OK) {
        return err;
    }

    buf[0] = INPUT_PROTOCOL_OPCODE_MOUSE_MOVE;
    write_i16_le(&buf[1], dx);
    write_i16_le(&buf[3], dy);
    return ESP_OK;
}

esp_err_t input_protocol_mouse_button(uint8_t button, bool pressed, uint8_t *buf, size_t len)
{
    esp_err_t err = require_buffer(buf, len, INPUT_PROTOCOL_MOUSE_BUTTON_SIZE);
    if (err != ESP_OK) {
        return err;
    }

    buf[0] = INPUT_PROTOCOL_OPCODE_MOUSE_BUTTON;
    buf[1] = button;
    buf[2] = pressed ? 1 : 0;
    return ESP_OK;
}

esp_err_t input_protocol_keyboard_key(uint8_t keycode, bool pressed, uint8_t *buf, size_t len)
{
    esp_err_t err = require_buffer(buf, len, INPUT_PROTOCOL_KEYBOARD_KEY_SIZE);
    if (err != ESP_OK) {
        return err;
    }

    buf[0] = INPUT_PROTOCOL_OPCODE_KEYBOARD_KEY;
    buf[1] = keycode;
    buf[2] = pressed ? 1 : 0;
    return ESP_OK;
}

esp_err_t input_protocol_joystick_axis(int16_t x, int16_t y, int16_t z, int16_t rz,
                                       uint8_t *buf, size_t len)
{
    esp_err_t err = require_buffer(buf, len, INPUT_PROTOCOL_JOYSTICK_AXIS_SIZE);
    if (err != ESP_OK) {
        return err;
    }

    buf[0] = INPUT_PROTOCOL_OPCODE_JOYSTICK_AXIS;
    write_i16_le(&buf[1], x);
    write_i16_le(&buf[3], y);
    write_i16_le(&buf[5], z);
    write_i16_le(&buf[7], rz);
    return ESP_OK;
}

esp_err_t input_protocol_joystick_button(uint8_t button, bool pressed, uint8_t *buf, size_t len)
{
    esp_err_t err = require_buffer(buf, len, INPUT_PROTOCOL_JOYSTICK_BUTTON_SIZE);
    if (err != ESP_OK) {
        return err;
    }

    buf[0] = INPUT_PROTOCOL_OPCODE_JOYSTICK_BUTTON;
    buf[1] = button;
    buf[2] = pressed ? 1 : 0;
    return ESP_OK;
}
