#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SECURE_ENVELOPE_MAGIC = 0xa5,
    SECURE_ENVELOPE_VERSION = 0x01,
    SECURE_ENVELOPE_HEADER_SIZE = 6,
    SECURE_ENVELOPE_TAG_SIZE = 16,
    SECURE_ENVELOPE_MAX_SIZE = 250,
};

esp_err_t secure_envelope_build(uint32_t sequence,
                                const uint8_t *command_payload,
                                size_t command_payload_len,
                                uint8_t *out,
                                size_t out_len,
                                size_t *written);

#ifdef __cplusplus
}
#endif
