/*
 * Copyright 2026 clesiorki2018
 * SPDX-License-Identifier: Apache-2.0
 */
#include "system_config/system_config.h"
#include "head_emitter_private_config.h"

static const system_config_security_t s_security_config = {
    .has_env = HEAD_EMITTER_CONFIG_HAS_ENV != 0,
    .receiver_has_mac = HEAD_EMITTER_RECEIVER_HAS_MAC != 0,
    .encryption_enabled = HEAD_EMITTER_ESP_NOW_ENCRYPTION_ENABLED != 0,
    .replay_protection_enabled = HEAD_EMITTER_APP_REPLAY_PROTECTION_ENABLED != 0,
    .sender_enabled = HEAD_EMITTER_SENDER_ENABLED != 0,
    .wifi_channel = HEAD_EMITTER_ESP_NOW_WIFI_CHANNEL,
    .sequence_window = HEAD_EMITTER_APP_SEQUENCE_WINDOW,
    .receiver_mac = {HEAD_EMITTER_RECEIVER_MAC_BYTES},
    .pmk = {HEAD_EMITTER_ESP_NOW_PMK_BYTES},
    .lmk = {HEAD_EMITTER_ESP_NOW_LMK_BYTES},
    .app_auth_key = {HEAD_EMITTER_APP_AUTH_KEY_BYTES},
    .sender_name = HEAD_EMITTER_SENDER_NAME,
    .sender_capabilities = HEAD_EMITTER_SENDER_CAPABILITIES,
    .sequence_namespace = HEAD_EMITTER_APP_SEQUENCE_NAMESPACE,
    .sequence_key = HEAD_EMITTER_APP_SEQUENCE_KEY,
};

const system_config_security_t *system_config_get_security(void)
{
    return &s_security_config;
}
