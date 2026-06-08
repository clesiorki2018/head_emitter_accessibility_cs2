/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sequence_store_init(void);
esp_err_t sequence_store_next(uint32_t *sequence);

#ifdef __cplusplus
}
#endif
