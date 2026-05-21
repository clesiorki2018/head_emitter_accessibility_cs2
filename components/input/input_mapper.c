#include "input/input_mapper.h"
#include <string.h>

#define INPUT_MAPPER_USED_CHANNEL_MASK 0x0fff

static bool bit_is_set(uint16_t mask, size_t index)
{
    return (mask & (uint16_t)(1u << index)) != 0;
}

void input_mapper_default_config(input_mapper_config_t *config)
{
    if (config == NULL) {
        return;
    }

    for (size_t index = 0; index < INPUT_MAPPER_CHANNEL_COUNT; ++index) {
        config->channel_actions[index] = INPUT_ACTION_NONE;
    }

    config->channel_actions[0] = INPUT_ACTION_MOUSE_LEFT;
    config->channel_actions[1] = INPUT_ACTION_MOUSE_RIGHT;
    config->channel_actions[2] = INPUT_ACTION_MOUSE_MIDDLE;
    config->channel_actions[3] = INPUT_ACTION_KEY_W;
    config->channel_actions[4] = INPUT_ACTION_KEY_Q;
    config->debounce_polls = INPUT_MAPPER_DEFAULT_DEBOUNCE_POLLS;
}

esp_err_t input_mapper_init(input_mapper_t *mapper, const input_mapper_config_t *config)
{
    if (mapper == NULL || config == NULL || config->debounce_polls == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(mapper, 0, sizeof(*mapper));
    mapper->config = *config;
    return ESP_OK;
}

esp_err_t input_mapper_update(input_mapper_t *mapper, uint16_t touched_mask,
                              input_event_t *events, size_t max_events,
                              size_t *event_count)
{
    if (mapper == NULL || event_count == NULL || (max_events > 0 && events == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    *event_count = 0;
    touched_mask &= INPUT_MAPPER_USED_CHANNEL_MASK;

    for (size_t channel = 0; channel < INPUT_MAPPER_CHANNEL_COUNT; ++channel) {
        bool touched = bit_is_set(touched_mask, channel);
        bool stable = bit_is_set(mapper->stable_mask, channel);
        input_action_t action = mapper->config.channel_actions[channel];

        if (touched == stable) {
            mapper->candidate_counts[channel] = 0;
            if (touched) {
                mapper->candidate_mask |= (uint16_t)(1u << channel);
            } else {
                mapper->candidate_mask &= (uint16_t)~(1u << channel);
            }
            continue;
        }

        if (touched != bit_is_set(mapper->candidate_mask, channel)) {
            mapper->candidate_counts[channel] = 0;
            if (touched) {
                mapper->candidate_mask |= (uint16_t)(1u << channel);
            } else {
                mapper->candidate_mask &= (uint16_t)~(1u << channel);
            }
        }

        ++mapper->candidate_counts[channel];
        if (mapper->candidate_counts[channel] < mapper->config.debounce_polls) {
            continue;
        }

        mapper->candidate_counts[channel] = 0;
        if (touched) {
            mapper->stable_mask |= (uint16_t)(1u << channel);
        } else {
            mapper->stable_mask &= (uint16_t)~(1u << channel);
        }

        if (action == INPUT_ACTION_NONE) {
            continue;
        }

        if (*event_count >= max_events) {
            return ESP_ERR_INVALID_SIZE;
        }

        events[*event_count] = (input_event_t){
            .action = action,
            .type = touched ? INPUT_EVENT_PRESSED : INPUT_EVENT_RELEASED,
        };
        ++(*event_count);
    }

    return ESP_OK;
}
