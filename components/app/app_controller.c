#include "app/app_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input_protocol/input_protocol.h"
#include "transport_espnow/espnow_transport.h"

static const char *TAG = "app_controller";

static esp_err_t send_payload(const uint8_t *payload, size_t len)
{
    esp_err_t err = espnow_transport_send(payload, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW send failed: %s", esp_err_to_name(err));
    }

    return err;
}

static esp_err_t app_send_startup_test(void)
{
    uint8_t payload[INPUT_PROTOCOL_JOYSTICK_AXIS_SIZE];
    esp_err_t err;

    err = input_protocol_mouse_move(8, 4, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    err = send_payload(payload, INPUT_PROTOCOL_MOUSE_MOVE_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    err = input_protocol_mouse_button(0, true, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    err = send_payload(payload, INPUT_PROTOCOL_MOUSE_BUTTON_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(80));

    err = input_protocol_mouse_button(0, false, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    err = send_payload(payload, INPUT_PROTOCOL_MOUSE_BUTTON_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    err = input_protocol_keyboard_key(0x04, true, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    err = send_payload(payload, INPUT_PROTOCOL_KEYBOARD_KEY_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(80));

    err = input_protocol_keyboard_key(0x04, false, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    err = send_payload(payload, INPUT_PROTOCOL_KEYBOARD_KEY_SIZE);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    err = input_protocol_joystick_axis(0, 0, 0, 0, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }

    return send_payload(payload, INPUT_PROTOCOL_JOYSTICK_AXIS_SIZE);
}

esp_err_t app_controller_init(void)
{
    esp_err_t err = espnow_transport_init();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Sending startup input test");
    err = app_send_startup_test();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Startup input test sent");
    return ESP_OK;
}
