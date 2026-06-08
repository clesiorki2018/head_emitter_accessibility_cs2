/*
 * Copyright 2026 clesiorki2018
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_CONFIG_ESP_NOW_KEY_SIZE 16
#define SYSTEM_CONFIG_APP_AUTH_KEY_SIZE 32
#define SYSTEM_CONFIG_MAC_SIZE 6

typedef struct {
    bool has_env;
    bool receiver_has_mac;
    bool encryption_enabled;
    bool replay_protection_enabled;
    bool sender_enabled;
    uint8_t wifi_channel;
    uint8_t sequence_window;
    uint8_t receiver_mac[SYSTEM_CONFIG_MAC_SIZE];
    uint8_t pmk[SYSTEM_CONFIG_ESP_NOW_KEY_SIZE];
    uint8_t lmk[SYSTEM_CONFIG_ESP_NOW_KEY_SIZE];
    uint8_t app_auth_key[SYSTEM_CONFIG_APP_AUTH_KEY_SIZE];
    const char *sender_name;
    const char *sender_capabilities;
    const char *sequence_namespace;
    const char *sequence_key;
} system_config_security_t;

const system_config_security_t *system_config_get_security(void);

#ifdef __cplusplus
}
#endif
