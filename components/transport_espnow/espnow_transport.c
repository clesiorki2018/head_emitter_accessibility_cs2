#include <string.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "system_config/system_config.h"
#include "transport_espnow/espnow_transport.h"

static const char *TAG = "espnow_transport";

static esp_err_t wifi_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    const system_config_security_t *config = system_config_get_security();
    return esp_wifi_set_channel(config->wifi_channel, WIFI_SECOND_CHAN_NONE);
}

static esp_err_t add_receiver_peer(void)
{
    const system_config_security_t *config = system_config_get_security();

    if (!config->receiver_has_mac) {
        ESP_LOGE(TAG, "Receiver MAC is not configured");
        return ESP_ERR_INVALID_STATE;
    }

    esp_now_peer_info_t peer = {
        .channel = config->wifi_channel,
        .ifidx = WIFI_IF_STA,
        .encrypt = config->encryption_enabled,
    };

    memcpy(peer.peer_addr, config->receiver_mac, sizeof(peer.peer_addr));
    if (config->encryption_enabled) {
        memcpy(peer.lmk, config->lmk, sizeof(peer.lmk));
    }

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        return ESP_OK;
    }

    return err;
}

esp_err_t espnow_transport_init(void)
{
    const system_config_security_t *config = system_config_get_security();

    if (!config->has_env) {
        ESP_LOGE(TAG, "Missing local .env configuration");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config->sender_enabled) {
        ESP_LOGE(TAG, "Combo sender is disabled in local configuration");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = wifi_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_now_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(err));
        return err;
    }

    if (config->encryption_enabled) {
        err = esp_now_set_pmk(config->pmk);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure ESP-NOW PMK: %s", esp_err_to_name(err));
            return err;
        }
    }

    err = add_receiver_peer();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add receiver peer: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ESP-NOW transport ready on channel %u", config->wifi_channel);
    return ESP_OK;
}

esp_err_t espnow_transport_send(const uint8_t *payload, size_t len)
{
    const system_config_security_t *config = system_config_get_security();

    if (payload == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (len > ESP_NOW_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    return esp_now_send(config->receiver_mac, payload, len);
}
