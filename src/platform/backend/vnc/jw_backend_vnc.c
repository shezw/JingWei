/*
    JingWei
    src/platform/backend/vnc jw_backend_vnc.c    2026-07-18

     ______     __  __     ______     ______     __     __
    /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
    \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
     \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
      \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com

    @link    : https://github.com/shezw/JingWei
    @author  : shezw
    @email   : hello@shezw.com
*/

#include "jw_internal.h"

#include <jingwei/core.h>
#include <jingwei/vnc.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define JW_VNC_INPUT_CAPACITY 1024U
#define JW_VNC_PENDING_EVENT_CAPACITY 8U

typedef struct jw_vnc_proxy_impl {
    jw_surface_t *surface;
    jw_vnc_backend_t *backend;
    jw_vnc_config_t config;
    char *desktop_name;
    char *password;
    jw_event_t pending_events[JW_VNC_PENDING_EVENT_CAPACITY];
    unsigned int pending_index;
    unsigned int pending_count;
    size_t password_length;
    int started;
    int pointer_initialized;
    int pointer_x;
    int pointer_y;
    uint32_t pointer_buttons;
} jw_vnc_proxy_impl_t;

static void jw_vnc_proxy_clear_password(
    char *password,
    size_t password_length)
{
    volatile unsigned char *bytes = (volatile unsigned char *)password;

    if (!password) {
        return;
    }
    while (password_length > 0U) {
        bytes[--password_length] = 0U;
    }
}

static void jw_vnc_proxy_release_config(jw_vnc_proxy_impl_t *impl)
{
    impl->desktop_name = NULL;
    jw_vnc_proxy_clear_password(impl->password, impl->password_length);
    impl->password = NULL;
    impl->password_length = 0U;
    impl->config.desktop_name = NULL;
    impl->config.password = NULL;
}

static int jw_vnc_proxy_init(jw_proxy_t *proxy)
{
    jw_vnc_proxy_impl_t *impl;

    if (!proxy || !proxy->impl || proxy->width <= 0 || proxy->height <= 0) {
        return -1;
    }
    impl = (jw_vnc_proxy_impl_t *)proxy->impl;
    impl->surface = jw_surface_create(
        (uint32_t)proxy->width,
        (uint32_t)proxy->height,
        JW_VNC_INPUT_CAPACITY);
    if (!impl->surface) {
        return -1;
    }
    impl->backend = jw_vnc_backend_create(impl->surface, &impl->config);
    if (!impl->backend ||
        jw_vnc_backend_start(impl->backend) != JW_STATUS_OK) {
        return -1;
    }
    impl->started = 1;
    return 0;
}

static void jw_vnc_proxy_deinit(jw_proxy_t *proxy)
{
    jw_vnc_proxy_impl_t *impl;

    if (!proxy || !proxy->impl) {
        return;
    }
    impl = (jw_vnc_proxy_impl_t *)proxy->impl;
    if (impl->backend) {
        if (impl->started) {
            (void)jw_vnc_backend_stop(impl->backend);
        }
        jw_vnc_backend_destroy(impl->backend);
        impl->backend = NULL;
    }
    impl->started = 0;
    jw_surface_destroy(impl->surface);
    impl->surface = NULL;
    jw_vnc_proxy_release_config(impl);
}

static int jw_vnc_proxy_frame_from_buffer(
    const jw_buffer_t *buffer,
    jw_frame_t *frame)
{
    size_t row_bytes;
    size_t size;

    if (!buffer || !buffer->pixels || buffer->width <= 0 ||
        buffer->height <= 0 || buffer->stride <= 0 ||
        buffer->format != JW_PIXEL_FORMAT_ARGB8888 ||
        (size_t)buffer->width > SIZE_MAX / sizeof(uint32_t)) {
        return -1;
    }
    row_bytes = (size_t)buffer->width * sizeof(uint32_t);
    if ((size_t)buffer->stride < row_bytes ||
        (size_t)(buffer->height - 1) >
            (SIZE_MAX - row_bytes) / (size_t)buffer->stride) {
        return -1;
    }
    size = (size_t)(buffer->height - 1) * (size_t)buffer->stride + row_bytes;
    frame->pixels = buffer->pixels;
    frame->size = size;
    frame->width = (uint32_t)buffer->width;
    frame->height = (uint32_t)buffer->height;
    frame->stride = (uint32_t)buffer->stride;
    frame->format = buffer->format;
    return 0;
}

static int jw_vnc_proxy_commit(jw_proxy_t *proxy, const jw_buffer_t *buffer)
{
    jw_vnc_proxy_impl_t *impl;
    jw_frame_t frame;

    if (!proxy || !proxy->impl ||
        jw_vnc_proxy_frame_from_buffer(buffer, &frame) != 0) {
        return -1;
    }
    impl = (jw_vnc_proxy_impl_t *)proxy->impl;
    if (!impl->started || !impl->backend ||
        frame.width != (uint32_t)proxy->width ||
        frame.height != (uint32_t)proxy->height) {
        return -1;
    }
    return jw_vnc_backend_publish_frame(
        impl->backend, &frame, NULL, 0U, NULL) == JW_STATUS_OK
        ? 0 : -1;
}

static int jw_vnc_proxy_commit_rects(
    jw_proxy_t *proxy,
    const jw_buffer_t *buffer,
    const jw_rect_t *rects,
    int rect_count)
{
    jw_vnc_proxy_impl_t *impl;
    jw_frame_t frame;
    jw_damage_t *damages = NULL;
    int index;
    int result;

    if (!proxy || !proxy->impl || rect_count < 0 ||
        (rect_count > 0 && !rects) ||
        jw_vnc_proxy_frame_from_buffer(buffer, &frame) != 0 ||
        frame.width != (uint32_t)proxy->width ||
        frame.height != (uint32_t)proxy->height) {
        return -1;
    }
    impl = (jw_vnc_proxy_impl_t *)proxy->impl;
    if (!impl->started || !impl->backend) {
        return -1;
    }
    if (rect_count > 0) {
        if ((size_t)rect_count > SIZE_MAX / sizeof(*damages)) {
            return -1;
        }
        damages = (jw_damage_t *)calloc((size_t)rect_count, sizeof(*damages));
        if (!damages) {
            return -1;
        }
        for (index = 0; index < rect_count; ++index) {
            if (rects[index].x < 0 || rects[index].y < 0 ||
                rects[index].w <= 0 || rects[index].h <= 0 ||
                rects[index].x >= proxy->width ||
                rects[index].y >= proxy->height ||
                rects[index].w > proxy->width - rects[index].x ||
                rects[index].h > proxy->height - rects[index].y) {
                free(damages);
                return -1;
            }
            damages[index].x = (uint32_t)rects[index].x;
            damages[index].y = (uint32_t)rects[index].y;
            damages[index].width = (uint32_t)rects[index].w;
            damages[index].height = (uint32_t)rects[index].h;
        }
    }

    result = jw_vnc_backend_publish_frame(
        impl->backend,
        &frame,
        damages,
        (size_t)rect_count,
        NULL) == JW_STATUS_OK ? 0 : -1;
    free(damages);
    return result;
}

static uint32_t jw_vnc_proxy_mouse_buttons(uint32_t rfb_buttons)
{
    uint32_t buttons = 0U;

    if ((rfb_buttons & 1U) != 0U) {
        buttons |= 1U << JW_MOUSE_LEFT;
    }
    if ((rfb_buttons & 2U) != 0U) {
        buttons |= 1U << JW_MOUSE_MIDDLE;
    }
    if ((rfb_buttons & 4U) != 0U) {
        buttons |= 1U << JW_MOUSE_RIGHT;
    }
    return buttons;
}

static int jw_vnc_proxy_queue_event(
    jw_vnc_proxy_impl_t *impl,
    const jw_event_t *event)
{
    if (impl->pending_count >= JW_VNC_PENDING_EVENT_CAPACITY) {
        return -1;
    }
    impl->pending_events[impl->pending_count++] = *event;
    return 0;
}

static int jw_vnc_proxy_queue_pointer(
    jw_vnc_proxy_impl_t *impl,
    const jw_input_event_t *input)
{
    static const uint32_t masks[] = { 1U, 2U, 4U };
    static const int buttons[] = {
        JW_MOUSE_LEFT,
        JW_MOUSE_MIDDLE,
        JW_MOUSE_RIGHT
    };
    jw_event_t event;
    uint32_t current_buttons = input->data.pointer.buttons;
    uint32_t mapped_buttons = jw_vnc_proxy_mouse_buttons(current_buttons);
    unsigned int index;

    impl->pending_index = 0U;
    impl->pending_count = 0U;
    if (!impl->pointer_initialized ||
        impl->pointer_x != (int)input->data.pointer.x ||
        impl->pointer_y != (int)input->data.pointer.y) {
        memset(&event, 0, sizeof(event));
        event.type = JW_EVENT_MOUSE_MOVE;
        event.timestamp_ms = jw_time_now_ms();
        event.data.mouse_move.x = (int)input->data.pointer.x;
        event.data.mouse_move.y = (int)input->data.pointer.y;
        event.data.mouse_move.dx = impl->pointer_initialized
            ? event.data.mouse_move.x - impl->pointer_x : 0;
        event.data.mouse_move.dy = impl->pointer_initialized
            ? event.data.mouse_move.y - impl->pointer_y : 0;
        event.data.mouse_move.buttons = mapped_buttons;
        if (jw_vnc_proxy_queue_event(impl, &event) != 0) {
            return -1;
        }
    }

    for (index = 0U; index < sizeof(masks) / sizeof(masks[0]); ++index) {
        int was_pressed = (impl->pointer_buttons & masks[index]) != 0U;
        int is_pressed = (current_buttons & masks[index]) != 0U;

        if (was_pressed == is_pressed) {
            continue;
        }
        memset(&event, 0, sizeof(event));
        event.type = JW_EVENT_MOUSE_KEY;
        event.timestamp_ms = jw_time_now_ms();
        event.data.mouse_key.button = buttons[index];
        event.data.mouse_key.buttons = mapped_buttons;
        event.data.mouse_key.state = is_pressed ? JW_BUTTON_DOWN : JW_BUTTON_UP;
        event.data.mouse_key.x = (int)input->data.pointer.x;
        event.data.mouse_key.y = (int)input->data.pointer.y;
        if (jw_vnc_proxy_queue_event(impl, &event) != 0) {
            return -1;
        }
    }

    if ((current_buttons & 8U) != 0U &&
        (impl->pointer_buttons & 8U) == 0U) {
        memset(&event, 0, sizeof(event));
        event.type = JW_EVENT_MOUSE_WHEEL;
        event.timestamp_ms = jw_time_now_ms();
        event.data.mouse_wheel.y = -1;
        event.data.mouse_wheel.mouse_x = (int)input->data.pointer.x;
        event.data.mouse_wheel.mouse_y = (int)input->data.pointer.y;
        if (jw_vnc_proxy_queue_event(impl, &event) != 0) {
            return -1;
        }
    }
    if ((current_buttons & 16U) != 0U &&
        (impl->pointer_buttons & 16U) == 0U) {
        memset(&event, 0, sizeof(event));
        event.type = JW_EVENT_MOUSE_WHEEL;
        event.timestamp_ms = jw_time_now_ms();
        event.data.mouse_wheel.y = 1;
        event.data.mouse_wheel.mouse_x = (int)input->data.pointer.x;
        event.data.mouse_wheel.mouse_y = (int)input->data.pointer.y;
        if (jw_vnc_proxy_queue_event(impl, &event) != 0) {
            return -1;
        }
    }

    impl->pointer_initialized = 1;
    impl->pointer_x = (int)input->data.pointer.x;
    impl->pointer_y = (int)input->data.pointer.y;
    impl->pointer_buttons = current_buttons;
    return 0;
}

static int jw_vnc_proxy_take_pending(
    jw_vnc_proxy_impl_t *impl,
    jw_event_t *event)
{
    if (impl->pending_index >= impl->pending_count) {
        impl->pending_index = 0U;
        impl->pending_count = 0U;
        return 0;
    }
    *event = impl->pending_events[impl->pending_index++];
    return 1;
}

static int jw_vnc_proxy_poll_event(jw_proxy_t *proxy, jw_event_t *event)
{
    jw_vnc_proxy_impl_t *impl;
    jw_input_event_t input;
    jw_status_t status;

    if (!proxy || !proxy->impl || !event) {
        return -1;
    }
    impl = (jw_vnc_proxy_impl_t *)proxy->impl;
    if (jw_vnc_proxy_take_pending(impl, event) > 0) {
        return 1;
    }
    if (!impl->started || !impl->backend || !impl->surface ||
        jw_vnc_backend_process_events(impl->backend, 0) != JW_STATUS_OK) {
        return -1;
    }

    while ((status = jw_surface_poll_input(impl->surface, &input)) ==
        JW_STATUS_OK) {
        memset(event, 0, sizeof(*event));
        switch (input.type) {
        case JW_INPUT_EVENT_POINTER:
            if (jw_vnc_proxy_queue_pointer(impl, &input) != 0) {
                return -1;
            }
            if (jw_vnc_proxy_take_pending(impl, event) > 0) {
                return 1;
            }
            break;
        case JW_INPUT_EVENT_KEY:
            event->type = JW_EVENT_KEY;
            event->timestamp_ms = jw_time_now_ms();
            event->data.key.key = (int)input.data.key.keysym;
            event->data.key.state = input.data.key.pressed
                ? JW_KEY_DOWN : JW_KEY_UP;
            return 1;
        case JW_INPUT_EVENT_RESET:
            impl->pointer_buttons = 0U;
            impl->pointer_initialized = 0;
            event->type = JW_EVENT_INPUT_RESET;
            event->timestamp_ms = jw_time_now_ms();
            return 1;
        default:
            return -1;
        }
    }
    return status == JW_STATUS_EMPTY ? 0 : -1;
}

static const jw_proxy_ops_t JW_VNC_PROXY_OPS = {
    jw_vnc_proxy_init,
    jw_vnc_proxy_deinit,
    jw_vnc_proxy_commit,
    jw_vnc_proxy_commit_rects,
    NULL,
    jw_vnc_proxy_poll_event
};

jw_proxy_t *jw_proxy_create_vnc(
    int width,
    int height,
    const jw_vnc_config_t *config)
{
    jw_vnc_proxy_impl_t *impl;
    jw_proxy_t *proxy;
    char *config_storage;
    size_t desktop_name_size = 0U;
    size_t password_size = 0U;
    size_t allocation_size;

    if (width <= 0 || height <= 0 || !jw_vnc_backend_is_available()) {
        return NULL;
    }
    if (config && config->desktop_name) {
        desktop_name_size = strlen(config->desktop_name) + 1U;
    }
    if (config && config->password) {
        password_size = strlen(config->password) + 1U;
    }
    if (desktop_name_size > SIZE_MAX - sizeof(*impl) ||
        password_size > SIZE_MAX - sizeof(*impl) - desktop_name_size) {
        return NULL;
    }
    allocation_size = sizeof(*impl) + desktop_name_size + password_size;
    impl = (jw_vnc_proxy_impl_t *)calloc(1, allocation_size);
    if (!impl) {
        return NULL;
    }
    config_storage = (char *)(impl + 1);
    if (config) {
        impl->config.port = config->port;
        if (desktop_name_size > 0U) {
            memcpy(config_storage, config->desktop_name, desktop_name_size);
            impl->desktop_name = config_storage;
            impl->config.desktop_name = impl->desktop_name;
            config_storage += desktop_name_size;
        }
        if (password_size > 0U) {
            memcpy(config_storage, config->password, password_size);
            impl->password = config_storage;
            impl->password_length = password_size - 1U;
            impl->config.password = impl->password;
        }
    }

    proxy = jw_proxy_alloc(&JW_VNC_PROXY_OPS, width, height, impl);
    if (!proxy) {
        jw_vnc_proxy_release_config(impl);
        free(impl);
    }
    return proxy;
}
