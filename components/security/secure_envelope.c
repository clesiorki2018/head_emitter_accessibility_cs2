/* SPDX-License-Identifier: Apache-2.0 */
#include "security/secure_envelope.h"
#include <stdbool.h>
#include <string.h>
#include "psa/crypto.h"
#include "system_config/system_config.h"

#define SECURE_ENVELOPE_HMAC_SIZE 32

static bool auth_key_is_configured(const system_config_security_t *config)
{
    uint8_t folded = 0;

    for (size_t index = 0; index < SYSTEM_CONFIG_APP_AUTH_KEY_SIZE; ++index) {
        folded |= config->app_auth_key[index];
    }

    return folded != 0;
}

static void write_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xffu);
    out[1] = (uint8_t)((value >> 8) & 0xffu);
    out[2] = (uint8_t)((value >> 16) & 0xffu);
    out[3] = (uint8_t)((value >> 24) & 0xffu);
}

esp_err_t secure_envelope_build(uint32_t sequence,
                                const uint8_t *command_payload,
                                size_t command_payload_len,
                                uint8_t *out,
                                size_t out_len,
                                size_t *written)
{
    const system_config_security_t *config = system_config_get_security();
    size_t packet_len = SECURE_ENVELOPE_HEADER_SIZE + command_payload_len + SECURE_ENVELOPE_TAG_SIZE;
    uint8_t digest[SECURE_ENVELOPE_HMAC_SIZE];

    if (command_payload == NULL || command_payload_len == 0 || out == NULL || written == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!auth_key_is_configured(config)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (packet_len > out_len || packet_len > SECURE_ENVELOPE_MAX_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    out[0] = SECURE_ENVELOPE_MAGIC;
    out[1] = SECURE_ENVELOPE_VERSION;
    write_u32_le(&out[2], sequence);
    memcpy(&out[SECURE_ENVELOPE_HEADER_SIZE], command_payload, command_payload_len);

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    size_t digest_len = 0;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, SYSTEM_CONFIG_APP_AUTH_KEY_SIZE * 8);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    psa_status_t status = psa_import_key(&attributes,
                                         config->app_auth_key,
                                         SYSTEM_CONFIG_APP_AUTH_KEY_SIZE,
                                         &key_id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        return ESP_FAIL;
    }

    status = psa_mac_compute(key_id,
                             PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             out,
                             packet_len - SECURE_ENVELOPE_TAG_SIZE,
                             digest,
                             sizeof(digest),
                             &digest_len);
    psa_destroy_key(key_id);
    if (status != PSA_SUCCESS || digest_len < SECURE_ENVELOPE_TAG_SIZE) {
        return ESP_FAIL;
    }

    memcpy(&out[packet_len - SECURE_ENVELOPE_TAG_SIZE], digest, SECURE_ENVELOPE_TAG_SIZE);
    *written = packet_len;
    return ESP_OK;
}
