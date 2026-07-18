#include "jw_internal.h"

uint32_t jw_pack_argb_for_format(uint32_t argb, jw_pixel_format_t format)
{
    uint32_t a = (argb >> 24) & 0xffu;
    uint32_t r = (argb >> 16) & 0xffu;
    uint32_t g = (argb >> 8) & 0xffu;
    uint32_t b = argb & 0xffu;

    switch (format) {
    case JW_PIXEL_FORMAT_ARGB8888:
        return (a << 24) | (r << 16) | (g << 8) | b;
    case JW_PIXEL_FORMAT_XRGB8888:
        return (0xffu << 24) | (r << 16) | (g << 8) | b;
    case JW_PIXEL_FORMAT_BGRA8888:
        return (b << 24) | (g << 16) | (r << 8) | a;
    case JW_PIXEL_FORMAT_BGRX8888:
        return (b << 24) | (g << 16) | (r << 8) | 0xffu;
    case JW_PIXEL_FORMAT_INVALID:
    default:
        return argb;
    }
}

uint32_t jw_unpack_argb_from_format(uint32_t pixel, jw_pixel_format_t format)
{
    uint32_t a;
    uint32_t r;
    uint32_t g;
    uint32_t b;

    switch (format) {
    case JW_PIXEL_FORMAT_ARGB8888:
        return pixel;
    case JW_PIXEL_FORMAT_XRGB8888:
        return pixel | 0xff000000u;
    case JW_PIXEL_FORMAT_BGRA8888:
        b = (pixel >> 24) & 0xffu;
        g = (pixel >> 16) & 0xffu;
        r = (pixel >> 8) & 0xffu;
        a = pixel & 0xffu;
        return (a << 24) | (r << 16) | (g << 8) | b;
    case JW_PIXEL_FORMAT_BGRX8888:
        b = (pixel >> 24) & 0xffu;
        g = (pixel >> 16) & 0xffu;
        r = (pixel >> 8) & 0xffu;
        return 0xff000000u | (r << 16) | (g << 8) | b;
    case JW_PIXEL_FORMAT_INVALID:
    default:
        return pixel;
    }
}

int jw_soft_fill_rect(jw_buffer_t *buffer, const jw_rect_t *rect, uint32_t argb)
{
    int x0;
    int y0;
    int x1;
    int y1;
    int x;
    int y;

    if (!buffer || !buffer->pixels || !rect) {
        return -1;
    }

    x0 = rect->x < 0 ? 0 : rect->x;
    y0 = rect->y < 0 ? 0 : rect->y;
    x1 = rect->x + rect->w;
    y1 = rect->y + rect->h;

    if (x1 > buffer->width) {
        x1 = buffer->width;
    }
    if (y1 > buffer->height) {
        y1 = buffer->height;
    }
    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }

    argb = jw_pack_argb_for_format(argb, buffer->format);
    for (y = y0; y < y1; ++y) {
        uint32_t *row = (uint32_t *)((unsigned char *)buffer->pixels + ((size_t)y * (size_t)buffer->stride));
        for (x = x0; x < x1; ++x) {
            row[x] = argb;
        }
    }

    return 0;
}
