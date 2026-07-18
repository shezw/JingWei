/**
    -----------------------------------------------------------

    Project JingWei
    core core.h    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#ifndef JINGWEI_CORE_H
#define JINGWEI_CORE_H

#include <stddef.h>
#include <stdint.h>

#include <jingwei.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum jw_status {
    JW_STATUS_OK = 0,
    JW_STATUS_INVALID_ARGUMENT = -1,
    JW_STATUS_UNSUPPORTED = -2,
    JW_STATUS_OUT_OF_MEMORY = -3,
    JW_STATUS_EMPTY = -4,
    JW_STATUS_FULL = -5,
    JW_STATUS_INVALID_STATE = -6,
    JW_STATUS_UNAVAILABLE = -7,
    JW_STATUS_IO_ERROR = -8
} jw_status_t;

typedef struct jw_frame {
    const void *pixels;
    size_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    jw_pixel_format_t format;
} jw_frame_t;

typedef struct jw_damage {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} jw_damage_t;

typedef enum jw_input_event_type {
    JW_INPUT_EVENT_POINTER = 1,
    JW_INPUT_EVENT_KEY = 2,
    /* Consumer must clear all held key and pointer-button state. */
    JW_INPUT_EVENT_RESET = 3
} jw_input_event_type_t;

typedef struct jw_pointer_event {
    uint32_t x;
    uint32_t y;
    uint32_t buttons;
} jw_pointer_event_t;

typedef struct jw_key_event {
    uint32_t keysym;
    int pressed;
} jw_key_event_t;

typedef struct jw_input_event {
    jw_input_event_type_t type;
    uint64_t event_id;
    union {
        jw_pointer_event_t pointer;
        jw_key_event_t key;
    } data;
} jw_input_event_t;

typedef struct jw_event_queue_stats {
    size_t capacity;
    size_t pending;
    /* Number of rejected, coalesced, or reset pending events. */
    uint64_t overflow_count;
    uint64_t next_event_id;
} jw_event_queue_stats_t;

typedef struct jw_surface_info {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    size_t size;
    jw_pixel_format_t format;
    uint64_t serial;
} jw_surface_info_t;

typedef struct jw_event_queue jw_event_queue_t;
typedef struct jw_surface jw_surface_t;

const char *jw_status_string(jw_status_t status);

jw_event_queue_t *jw_event_queue_create(size_t capacity);
void jw_event_queue_destroy(jw_event_queue_t *queue);
/*
 * A full queue coalesces a trailing pointer move. If a key or pointer-button
 * release cannot be retained without losing another release, all pending
 * events are replaced by one JW_INPUT_EVENT_RESET. RESET means that every key
 * and pointer button is released; its union payload is unused.
 */
jw_status_t jw_event_queue_push(
    jw_event_queue_t *queue,
    const jw_input_event_t *event,
    uint64_t *event_id);
jw_status_t jw_event_queue_pop(
    jw_event_queue_t *queue,
    jw_input_event_t *event);
jw_status_t jw_event_queue_get_stats(
    const jw_event_queue_t *queue,
    jw_event_queue_stats_t *stats);

/* Dimensions must be positive; owned ARGB storage is capped at INT_MAX bytes. */
jw_surface_t *jw_surface_create(
    uint32_t width,
    uint32_t height,
    size_t input_capacity);
void jw_surface_destroy(jw_surface_t *surface);
jw_status_t jw_surface_get_info(
    const jw_surface_t *surface,
    jw_surface_info_t *info);

/*
 * A zero damage_count means that the whole frame is damaged. The first
 * successful submission always copies the whole frame, regardless of damage.
 */
jw_status_t jw_surface_submit_frame(
    jw_surface_t *surface,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count,
    uint64_t *serial);

/*
 * Atomically submits a frame and copies that exact committed serial into the
 * caller-owned destination. The destination must not overlap frame->pixels.
 */
jw_status_t jw_surface_submit_frame_and_copy(
    jw_surface_t *surface,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count,
    void *pixels,
    size_t size,
    uint32_t stride,
    uint64_t *serial);

jw_status_t jw_surface_copy_front_buffer(
    const jw_surface_t *surface,
    void *pixels,
    size_t size,
    uint32_t stride,
    uint64_t *serial);

jw_status_t jw_surface_enqueue_input(
    jw_surface_t *surface,
    const jw_input_event_t *event,
    uint64_t *event_id);
jw_status_t jw_surface_poll_input(
    jw_surface_t *surface,
    jw_input_event_t *event);
jw_status_t jw_surface_get_input_stats(
    const jw_surface_t *surface,
    jw_event_queue_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
