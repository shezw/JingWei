/**
    -----------------------------------------------------------

    Project JingWei
    core event_queue.c    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#include "core_internal.h"

#include <stdlib.h>
#include <string.h>

const char *jw_status_string(jw_status_t status)
{
    switch (status) {
    case JW_STATUS_OK:
        return "ok";
    case JW_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case JW_STATUS_UNSUPPORTED:
        return "unsupported";
    case JW_STATUS_OUT_OF_MEMORY:
        return "out of memory";
    case JW_STATUS_EMPTY:
        return "empty";
    case JW_STATUS_FULL:
        return "full";
    case JW_STATUS_INVALID_STATE:
        return "invalid state";
    case JW_STATUS_UNAVAILABLE:
        return "unavailable";
    case JW_STATUS_IO_ERROR:
        return "I/O error";
    default:
        return "unknown";
    }
}

jw_event_queue_t *jw_event_queue_create(size_t capacity)
{
    jw_event_queue_t *queue;

    if (capacity == 0 || capacity > SIZE_MAX / sizeof(*queue->events)) {
        return NULL;
    }

    queue = (jw_event_queue_t *)calloc(1, sizeof(*queue));
    if (queue == NULL) {
        return NULL;
    }

    queue->events = (jw_input_event_t *)calloc(capacity, sizeof(*queue->events));
    if (queue->events == NULL) {
        free(queue);
        return NULL;
    }

    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->events);
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    queue->next_event_id = 1;
    return queue;
}

void jw_event_queue_destroy(jw_event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    pthread_mutex_destroy(&queue->mutex);
    free(queue->events);
    free(queue);
}

jw_status_t jw_event_queue_push(
    jw_event_queue_t *queue,
    const jw_input_event_t *event,
    uint64_t *event_id)
{
    size_t tail;
    size_t dropped;
    int pointer_release;
    int protected_release;
    int force_reset;
    jw_input_event_t queued_event;

    if (queue == NULL || event == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    if (event->type != JW_INPUT_EVENT_POINTER &&
        event->type != JW_INPUT_EVENT_KEY &&
        event->type != JW_INPUT_EVENT_RESET) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    if (event->type == JW_INPUT_EVENT_KEY &&
        event->data.key.pressed != 0 && event->data.key.pressed != 1) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&queue->mutex);
    pointer_release = event->type == JW_INPUT_EVENT_POINTER &&
        queue->has_pointer_state &&
        (queue->last_pointer_buttons & ~event->data.pointer.buttons) != 0;
    protected_release =
        (event->type == JW_INPUT_EVENT_KEY && event->data.key.pressed == 0) ||
        pointer_release;
    force_reset = event->type == JW_INPUT_EVENT_RESET;

    if (queue->count == queue->capacity) {
        tail = (queue->head + queue->count - 1U) % queue->capacity;
        if (event->type == JW_INPUT_EVENT_POINTER && !pointer_release &&
            queue->events[tail].type == JW_INPUT_EVENT_POINTER &&
            queue->events[tail].data.pointer.buttons ==
                event->data.pointer.buttons) {
            queued_event = *event;
            queued_event.event_id = queue->next_event_id;
            queue->next_event_id += 1;
            queue->events[tail] = queued_event;
            queue->last_pointer_buttons = event->data.pointer.buttons;
            queue->has_pointer_state = 1;
            queue->overflow_count += 1;
            pthread_mutex_unlock(&queue->mutex);
            if (event_id != NULL) {
                *event_id = queued_event.event_id;
            }
            return JW_STATUS_OK;
        }
        if (!protected_release) {
            if (!force_reset) {
                queue->overflow_count += 1;
                pthread_mutex_unlock(&queue->mutex);
                return JW_STATUS_FULL;
            }
        } else {
            force_reset = 1;
        }
    }

    if (force_reset) {
        dropped = queue->count;
        queue->head = 0;
        queue->count = 0;
        queue->overflow_count += dropped;
        memset(&queued_event, 0, sizeof(queued_event));
        queued_event.type = JW_INPUT_EVENT_RESET;
    } else {
        queued_event = *event;
    }

    queued_event.event_id = queue->next_event_id;
    queue->next_event_id += 1;
    tail = (queue->head + queue->count) % queue->capacity;
    queue->events[tail] = queued_event;
    queue->count += 1;
    if (queued_event.type == JW_INPUT_EVENT_RESET) {
        queue->last_pointer_buttons = 0;
        queue->has_pointer_state = 1;
    } else if (event->type == JW_INPUT_EVENT_POINTER) {
        queue->last_pointer_buttons = event->data.pointer.buttons;
        queue->has_pointer_state = 1;
    }
    pthread_mutex_unlock(&queue->mutex);

    if (event_id != NULL) {
        *event_id = queued_event.event_id;
    }
    return JW_STATUS_OK;
}

jw_status_t jw_event_queue_pop(
    jw_event_queue_t *queue,
    jw_input_event_t *event)
{
    if (queue == NULL || event == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&queue->mutex);
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return JW_STATUS_EMPTY;
    }

    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count -= 1;
    pthread_mutex_unlock(&queue->mutex);
    return JW_STATUS_OK;
}

jw_status_t jw_event_queue_get_stats(
    const jw_event_queue_t *queue,
    jw_event_queue_stats_t *stats)
{
    jw_event_queue_t *mutable_queue;

    if (queue == NULL || stats == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    mutable_queue = (jw_event_queue_t *)queue;
    pthread_mutex_lock(&mutable_queue->mutex);
    stats->capacity = queue->capacity;
    stats->pending = queue->count;
    stats->overflow_count = queue->overflow_count;
    stats->next_event_id = queue->next_event_id;
    pthread_mutex_unlock(&mutable_queue->mutex);
    return JW_STATUS_OK;
}
