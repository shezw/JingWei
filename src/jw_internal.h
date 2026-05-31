#ifndef JW_INTERNAL_H
#define JW_INTERNAL_H

#include "jingwei.h"

#include <stddef.h>

typedef struct jw_proxy_ops {
    int (*init)(jw_proxy_t *proxy);
    void (*deinit)(jw_proxy_t *proxy);
    int (*commit)(jw_proxy_t *proxy, const jw_buffer_t *buffer);
    int (*commit_rects)(jw_proxy_t *proxy, const jw_buffer_t *buffer, const jw_rect_t *rects, int rect_count);
    int (*resize)(jw_proxy_t *proxy, int width, int height);
    int (*poll_event)(jw_proxy_t *proxy, jw_event_t *event);
} jw_proxy_ops_t;

struct jw_buffer {
    int width;
    int height;
    int stride;
    jw_pixel_format_t format;
    void *pixels;
    int owns_pixels;
    jw_buffer_release_t release;
    void *release_user_data;
};

struct jw_proxy {
    const jw_proxy_ops_t *ops;
    int width;
    int height;
    void *impl;
};

struct jw_event_manager {
    jw_proxy_t *proxy;
};

struct jw_display {
    int id;
    int width;
    int height;
    jw_buffer_t *framebuffer;
    int owns_framebuffer;
    jw_proxy_t *proxy;
    jw_event_manager_t *event_manager;
};

struct jw_context {
    jw_display_t **displays;
    int display_count;
    int display_capacity;
    int running;
    jw_event_callback_t event_callback;
    void *event_user_data;
};

uint64_t jw_time_now_ms(void);
void jw_sleep_ms(unsigned int ms);
int jw_soft_fill_rect(jw_buffer_t *buffer, const jw_rect_t *rect, uint32_t argb);
uint32_t jw_pack_argb_for_format(uint32_t argb, jw_pixel_format_t format);
uint32_t jw_unpack_argb_from_format(uint32_t pixel, jw_pixel_format_t format);
jw_proxy_t *jw_proxy_alloc(const jw_proxy_ops_t *ops, int width, int height, void *impl);
int jw_event_manager_poll(jw_event_manager_t *manager, jw_event_t *event);

#endif
