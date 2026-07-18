/**
    -----------------------------------------------------------

    Project JingWei
    core surface.c    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#include "core_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define JW_ARGB8888_BYTES_PER_PIXEL 4U

static int jw_buffer_size(
    uint32_t width,
    uint32_t height,
    uint32_t *stride,
    size_t *size)
{
    uint32_t row_bytes;

    if (width == 0 || height == 0 ||
        width > UINT32_MAX / JW_ARGB8888_BYTES_PER_PIXEL) {
        return 0;
    }

    row_bytes = width * JW_ARGB8888_BYTES_PER_PIXEL;
    if ((size_t)height > SIZE_MAX / row_bytes) {
        return 0;
    }

    *size = (size_t)row_bytes * height;
    if (*size > (size_t)INT_MAX) {
        return 0;
    }
    *stride = row_bytes;
    return 1;
}

static int jw_required_size(
    uint32_t height,
    uint32_t stride,
    uint32_t row_bytes,
    size_t *size)
{
    size_t prefix;

    if (height == 0 || stride < row_bytes ||
        (size_t)(height - 1U) > (SIZE_MAX - row_bytes) / stride) {
        return 0;
    }

    prefix = (size_t)(height - 1U) * stride;
    *size = prefix + row_bytes;
    return 1;
}

static int jw_damage_is_valid(
    const jw_surface_t *surface,
    const jw_damage_t *damage)
{
    return damage->width != 0 && damage->height != 0 &&
        damage->x < surface->width && damage->y < surface->height &&
        damage->width <= surface->width - damage->x &&
        damage->height <= surface->height - damage->y;
}

static jw_status_t jw_validate_frame(
    const jw_surface_t *surface,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count)
{
    size_t required_size;
    size_t index;

    if (surface == NULL || frame == NULL || frame->pixels == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    if (frame->format != JW_PIXEL_FORMAT_ARGB8888) {
        return JW_STATUS_UNSUPPORTED;
    }
    if (frame->width != surface->width || frame->height != surface->height ||
        (damage_count > 0 && damages == NULL)) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    if (!jw_required_size(
            frame->height, frame->stride, surface->stride, &required_size) ||
        frame->size < required_size) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0; index < damage_count; ++index) {
        if (!jw_damage_is_valid(surface, &damages[index])) {
            return JW_STATUS_INVALID_ARGUMENT;
        }
    }
    return JW_STATUS_OK;
}

static void jw_copy_frame_locked(
    jw_surface_t *surface,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count)
{
    size_t index;
    uint32_t row;
    int full_frame = surface->serial == 0 || damage_count == 0;

    if (full_frame) {
        for (row = 0; row < surface->height; ++row) {
            memcpy(
                surface->front_buffer + (size_t)row * surface->stride,
                (const uint8_t *)frame->pixels + (size_t)row * frame->stride,
                surface->stride);
        }
        return;
    }

    for (index = 0; index < damage_count; ++index) {
        const jw_damage_t *damage = &damages[index];
        size_t copy_size = (size_t)damage->width * JW_ARGB8888_BYTES_PER_PIXEL;
        size_t x_offset = (size_t)damage->x * JW_ARGB8888_BYTES_PER_PIXEL;

        for (row = damage->y; row < damage->y + damage->height; ++row) {
            memcpy(
                surface->front_buffer + (size_t)row * surface->stride + x_offset,
                (const uint8_t *)frame->pixels + (size_t)row * frame->stride + x_offset,
                copy_size);
        }
    }
}

static void jw_copy_front_buffer_locked(
    const jw_surface_t *surface,
    void *pixels,
    uint32_t stride)
{
    uint32_t row;

    for (row = 0; row < surface->height; ++row) {
        memcpy(
            (uint8_t *)pixels + (size_t)row * stride,
            surface->front_buffer + (size_t)row * surface->stride,
            surface->stride);
    }
}

jw_surface_t *jw_surface_create(
    uint32_t width,
    uint32_t height,
    size_t input_capacity)
{
    jw_surface_t *surface;
    uint32_t stride;
    size_t size;

    if (input_capacity == 0 || !jw_buffer_size(width, height, &stride, &size)) {
        return NULL;
    }

    surface = (jw_surface_t *)calloc(1, sizeof(*surface));
    if (surface == NULL) {
        return NULL;
    }

    surface->front_buffer = (uint8_t *)calloc(1, size);
    if (surface->front_buffer == NULL) {
        free(surface);
        return NULL;
    }

    surface->input_queue = jw_event_queue_create(input_capacity);
    if (surface->input_queue == NULL) {
        free(surface->front_buffer);
        free(surface);
        return NULL;
    }

    if (pthread_mutex_init(&surface->frame_mutex, NULL) != 0) {
        jw_event_queue_destroy(surface->input_queue);
        free(surface->front_buffer);
        free(surface);
        return NULL;
    }

    surface->width = width;
    surface->height = height;
    surface->stride = stride;
    surface->size = size;
    return surface;
}

void jw_surface_destroy(jw_surface_t *surface)
{
    if (surface == NULL) {
        return;
    }

    pthread_mutex_destroy(&surface->frame_mutex);
    jw_event_queue_destroy(surface->input_queue);
    free(surface->front_buffer);
    free(surface);
}

jw_status_t jw_surface_get_info(
    const jw_surface_t *surface,
    jw_surface_info_t *info)
{
    jw_surface_t *mutable_surface;

    if (surface == NULL || info == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    mutable_surface = (jw_surface_t *)surface;
    pthread_mutex_lock(&mutable_surface->frame_mutex);
    info->width = surface->width;
    info->height = surface->height;
    info->stride = surface->stride;
    info->size = surface->size;
    info->format = JW_PIXEL_FORMAT_ARGB8888;
    info->serial = surface->serial;
    pthread_mutex_unlock(&mutable_surface->frame_mutex);
    return JW_STATUS_OK;
}

jw_status_t jw_surface_submit_frame(
    jw_surface_t *surface,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count,
    uint64_t *serial)
{
    jw_status_t status;

    status = jw_validate_frame(surface, frame, damages, damage_count);
    if (status != JW_STATUS_OK) {
        return status;
    }

    pthread_mutex_lock(&surface->frame_mutex);
    jw_copy_frame_locked(surface, frame, damages, damage_count);
    surface->serial += 1;
    if (serial != NULL) {
        *serial = surface->serial;
    }
    pthread_mutex_unlock(&surface->frame_mutex);
    return JW_STATUS_OK;
}

jw_status_t jw_surface_submit_frame_and_copy(
    jw_surface_t *surface,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count,
    void *pixels,
    size_t size,
    uint32_t stride,
    uint64_t *serial)
{
    jw_status_t status;
    size_t required_size;

    status = jw_validate_frame(surface, frame, damages, damage_count);
    if (status != JW_STATUS_OK) {
        return status;
    }
    if (pixels == NULL ||
        !jw_required_size(surface->height, stride, surface->stride, &required_size) ||
        size < required_size) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&surface->frame_mutex);
    jw_copy_frame_locked(surface, frame, damages, damage_count);
    surface->serial += 1;
    jw_copy_front_buffer_locked(surface, pixels, stride);
    if (serial != NULL) {
        *serial = surface->serial;
    }
    pthread_mutex_unlock(&surface->frame_mutex);
    return JW_STATUS_OK;
}

jw_status_t jw_surface_copy_front_buffer(
    const jw_surface_t *surface,
    void *pixels,
    size_t size,
    uint32_t stride,
    uint64_t *serial)
{
    jw_surface_t *mutable_surface;
    size_t required_size;

    if (surface == NULL || pixels == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    if (!jw_required_size(
            surface->height, stride, surface->stride, &required_size) ||
        size < required_size) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    mutable_surface = (jw_surface_t *)surface;
    pthread_mutex_lock(&mutable_surface->frame_mutex);
    jw_copy_front_buffer_locked(surface, pixels, stride);
    if (serial != NULL) {
        *serial = surface->serial;
    }
    pthread_mutex_unlock(&mutable_surface->frame_mutex);
    return JW_STATUS_OK;
}

jw_status_t jw_surface_enqueue_input(
    jw_surface_t *surface,
    const jw_input_event_t *event,
    uint64_t *event_id)
{
    if (surface == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    return jw_event_queue_push(surface->input_queue, event, event_id);
}

jw_status_t jw_surface_poll_input(
    jw_surface_t *surface,
    jw_input_event_t *event)
{
    if (surface == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    return jw_event_queue_pop(surface->input_queue, event);
}

jw_status_t jw_surface_get_input_stats(
    const jw_surface_t *surface,
    jw_event_queue_stats_t *stats)
{
    if (surface == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    return jw_event_queue_get_stats(surface->input_queue, stats);
}
