/*
 * Copyright 2026 clesiorki2018
 * SPDX-License-Identifier: Apache-2.0
 */
#include "input/input_mapper.h"
#include <string.h>

#define INPUT_MAPPER_USED_CHANNEL_MASK 0x0fff
#define INPUT_MAPPER_NO_LOCKED_CHANNEL INPUT_MAPPER_CHANNEL_COUNT

static bool bit_is_set(uint16_t mask, size_t index)
{
    return (mask & (uint16_t)(1u << index)) != 0;
}

static uint8_t first_touched_channel(uint16_t touched_mask)
{
    for (uint8_t channel = 0; channel < INPUT_MAPPER_CHANNEL_COUNT; ++channel) {
        if (bit_is_set(touched_mask, channel)) {
            return channel;
        }
    }

    return INPUT_MAPPER_NO_LOCKED_CHANNEL;
}

static uint16_t action_channel_mask(const input_mapper_config_t *config)
{
    uint16_t mask = 0;

    for (uint8_t channel = 0; channel < INPUT_MAPPER_CHANNEL_COUNT; ++channel) {
        if (config->channel_actions[channel] != INPUT_ACTION_NONE) {
            mask |= (uint16_t)(1u << channel);
        }
    }

    return mask;
}

void input_mapper_default_config(input_mapper_config_t *config)
{
    if (config == NULL) {
        return;
    }

    for (size_t index = 0; index < INPUT_MAPPER_CHANNEL_COUNT; ++index) {
        config->channel_actions[index] = INPUT_ACTION_NONE;
    }

    config->channel_actions[7] = INPUT_ACTION_MOUSE_LEFT;
    config->channel_actions[9] = INPUT_ACTION_MOUSE_RIGHT;
    config->channel_actions[10] = INPUT_ACTION_MOUSE_MIDDLE;
    config->channel_actions[11] = INPUT_ACTION_KEY_Q;
    config->debounce_polls = INPUT_MAPPER_DEFAULT_DEBOUNCE_POLLS;
}

esp_err_t input_mapper_init(input_mapper_t *mapper, const input_mapper_config_t *config)
{
    if (mapper == NULL || config == NULL || config->debounce_polls == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(mapper, 0, sizeof(*mapper));
    mapper->config = *config;
    mapper->locked_channel = INPUT_MAPPER_NO_LOCKED_CHANNEL;
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
    touched_mask &= action_channel_mask(&mapper->config);

    if (touched_mask == 0) {
        mapper->locked_channel = INPUT_MAPPER_NO_LOCKED_CHANNEL;
    } else if (mapper->locked_channel == INPUT_MAPPER_NO_LOCKED_CHANNEL) {
        mapper->locked_channel = first_touched_channel(touched_mask);
    }

    if (mapper->locked_channel != INPUT_MAPPER_NO_LOCKED_CHANNEL) {
        touched_mask &= (uint16_t)(1u << mapper->locked_channel);
    }

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

esp_err_t input_mapper_release_all(input_mapper_t *mapper,
                                   input_event_t *events,
                                   size_t max_events,
                                   size_t *event_count)
{
    if (mapper == NULL || event_count == NULL || (max_events > 0 && events == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    *event_count = 0;

    for (size_t channel = 0; channel < INPUT_MAPPER_CHANNEL_COUNT; ++channel) {
        if (!bit_is_set(mapper->stable_mask, channel)) {
            continue;
        }

        input_action_t action = mapper->config.channel_actions[channel];
        if (action == INPUT_ACTION_NONE) {
            continue;
        }

        if (*event_count >= max_events) {
            return ESP_ERR_INVALID_SIZE;
        }

        events[*event_count] = (input_event_t){
            .action = action,
            .type = INPUT_EVENT_RELEASED,
        };
        ++(*event_count);
    }

    mapper->stable_mask = 0;
    mapper->candidate_mask = 0;
    mapper->locked_channel = INPUT_MAPPER_NO_LOCKED_CHANNEL;
    memset(mapper->candidate_counts, 0, sizeof(mapper->candidate_counts));

    return ESP_OK;
}

bool input_mapper_has_mapped_touch(const input_mapper_t *mapper, uint16_t touched_mask)
{
    if (mapper == NULL) {
        return false;
    }

    touched_mask &= INPUT_MAPPER_USED_CHANNEL_MASK;
    touched_mask &= action_channel_mask(&mapper->config);
    return touched_mask != 0;
}
