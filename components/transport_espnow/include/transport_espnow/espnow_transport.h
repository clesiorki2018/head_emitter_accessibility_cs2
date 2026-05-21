#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t espnow_transport_init(void);
esp_err_t espnow_transport_send(const uint8_t *payload, size_t len);

#ifdef __cplusplus
}
#endif
