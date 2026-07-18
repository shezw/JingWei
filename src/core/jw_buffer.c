#include "jw_internal.h"

#include <stdlib.h>
#include <string.h>

jw_buffer_t *jw_buffer_create(int width, int height)
{
    jw_buffer_t *buffer;

    if (width <= 0 || height <= 0) {
        return NULL;
    }

    buffer = (jw_buffer_t *)calloc(1, sizeof(*buffer));
    if (!buffer) {
        return NULL;
    }

    buffer->width = width;
    buffer->height = height;
    buffer->stride = width * (int)sizeof(uint32_t);
    buffer->format = JW_PIXEL_FORMAT_ARGB8888;
    buffer->owns_pixels = 1;
    buffer->pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!buffer->pixels) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

jw_buffer_t *jw_buffer_wrap_pixels(int width,
                                   int height,
                                   int stride,
                                   jw_pixel_format_t format,
                                   void *pixels,
                                   jw_buffer_release_t release,
                                   void *user_data)
{
    jw_buffer_t *buffer;

    if (width <= 0 || height <= 0 || stride <= 0 || !pixels || format == JW_PIXEL_FORMAT_INVALID) {
        return NULL;
    }

    buffer = (jw_buffer_t *)calloc(1, sizeof(*buffer));
    if (!buffer) {
        return NULL;
    }

    buffer->width = width;
    buffer->height = height;
    buffer->stride = stride;
    buffer->format = format;
    buffer->pixels = pixels;
    buffer->release = release;
    buffer->release_user_data = user_data;
    return buffer;
}

void jw_buffer_destroy(jw_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }

    if (buffer->release) {
        buffer->release(buffer->pixels, buffer->release_user_data);
    } else if (buffer->owns_pixels) {
        free(buffer->pixels);
    }
    free(buffer);
}

int jw_buffer_width(const jw_buffer_t *buffer)
{
    return buffer ? buffer->width : 0;
}

int jw_buffer_height(const jw_buffer_t *buffer)
{
    return buffer ? buffer->height : 0;
}

int jw_buffer_stride(const jw_buffer_t *buffer)
{
    return buffer ? buffer->stride : 0;
}

jw_pixel_format_t jw_buffer_format(const jw_buffer_t *buffer)
{
    return buffer ? buffer->format : JW_PIXEL_FORMAT_INVALID;
}

void *jw_buffer_data(jw_buffer_t *buffer)
{
    return buffer ? buffer->pixels : NULL;
}

const void *jw_buffer_const_data(const jw_buffer_t *buffer)
{
    return buffer ? buffer->pixels : NULL;
}

int jw_buffer_fill(jw_buffer_t *buffer, uint32_t argb)
{
    jw_rect_t rect;

    if (!buffer) {
        return -1;
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = buffer->width;
    rect.h = buffer->height;
    return jw_soft_fill_rect(buffer, &rect, argb);
}

int jw_buffer_fill_rect(jw_buffer_t *buffer, const jw_rect_t *rect, uint32_t argb)
{
    if (!buffer || !rect) {
        return -1;
    }

    return jw_soft_fill_rect(buffer, rect, argb);
}
