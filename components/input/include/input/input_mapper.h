#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "input/input_action.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_MAPPER_CHANNEL_COUNT 12
#define INPUT_MAPPER_DEFAULT_DEBOUNCE_POLLS 3

typedef struct {
    input_action_t channel_actions[INPUT_MAPPER_CHANNEL_COUNT];
    uint8_t debounce_polls;
} input_mapper_config_t;

typedef struct {
    input_mapper_config_t config;
    uint16_t stable_mask;
    uint16_t candidate_mask;
    uint8_t locked_channel;
    uint8_t candidate_counts[INPUT_MAPPER_CHANNEL_COUNT];
} input_mapper_t;

void input_mapper_default_config(input_mapper_config_t *config);
esp_err_t input_mapper_init(input_mapper_t *mapper, const input_mapper_config_t *config);
esp_err_t input_mapper_update(input_mapper_t *mapper, uint16_t touched_mask,
                              input_event_t *events, size_t max_events,
                              size_t *event_count);
esp_err_t input_mapper_release_all(input_mapper_t *mapper,
                                   input_event_t *events,
                                   size_t max_events,
                                   size_t *event_count);
bool input_mapper_has_mapped_touch(const input_mapper_t *mapper, uint16_t touched_mask);

#ifdef __cplusplus
}
#endif
