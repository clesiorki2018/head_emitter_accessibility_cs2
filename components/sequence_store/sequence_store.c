/* SPDX-License-Identifier: Apache-2.0 */
#include "sequence_store/sequence_store.h"
#include <stdbool.h>
#include "nvs.h"
#include "system_config/system_config.h"

static nvs_handle_t s_nvs;
static bool s_initialized;
static uint32_t s_next_sequence;

esp_err_t sequence_store_init(void)
{
    const system_config_security_t *config = system_config_get_security();

    if (config->sequence_namespace == NULL || config->sequence_key == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = nvs_open(config->sequence_namespace, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_u32(s_nvs, config->sequence_key, &s_next_sequence);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_next_sequence = 1;
    } else if (err != ESP_OK) {
        nvs_close(s_nvs);
        s_nvs = 0;
        return err;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t sequence_store_next(uint32_t *sequence)
{
    const system_config_security_t *config = system_config_get_security();

    if (!s_initialized || sequence == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t current = s_next_sequence;
    uint32_t next = current + 1;
    esp_err_t err = nvs_set_u32(s_nvs, config->sequence_key, next);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_commit(s_nvs);
    if (err != ESP_OK) {
        return err;
    }

    s_next_sequence = next;
    *sequence = current;
    return ESP_OK;
}
