#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_ACTION_MOUSE_LEFT,
    INPUT_ACTION_MOUSE_RIGHT,
    INPUT_ACTION_MOUSE_MIDDLE,
    INPUT_ACTION_KEY_W,
    INPUT_ACTION_KEY_Q,
    INPUT_ACTION_NONE,
} input_action_t;

typedef enum {
    INPUT_EVENT_RELEASED = 0,
    INPUT_EVENT_PRESSED,
} input_event_type_t;

typedef struct {
    input_action_t action;
    input_event_type_t type;
} input_event_t;

static inline bool input_event_is_pressed(const input_event_t *event)
{
    return event != NULL && event->type == INPUT_EVENT_PRESSED;
}

#ifdef __cplusplus
}
#endif
