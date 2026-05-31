#include "jingwei.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 480

static uint32_t pack_bgra(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ((uint32_t)b << 24) | ((uint32_t)g << 16) | ((uint32_t)r << 8) | a;
}

static void fill_pattern(uint32_t *pixels, int width, int height, int cursor_x, int cursor_y)
{
    int x;
    int y;

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            uint8_t r = (uint8_t)((x * 255) / width);
            uint8_t g = (uint8_t)((y * 255) / height);
            uint8_t b = 40;
            pixels[(size_t)y * (size_t)width + (size_t)x] = pack_bgra(r, g, b, 255);
        }
    }

    for (y = cursor_y - 24; y < cursor_y + 24; ++y) {
        if (y < 0 || y >= height) {
            continue;
        }
        for (x = cursor_x - 24; x < cursor_x + 24; ++x) {
            if (x < 0 || x >= width) {
                continue;
            }
            pixels[(size_t)y * (size_t)width + (size_t)x] = pack_bgra(255, 255, 255, 255);
        }
    }
}

static void handle_event(jw_context_t *ctx, jw_display_t *display, const jw_event_t *event, void *user_data)
{
    uint32_t *pixels = (uint32_t *)user_data;
    jw_buffer_t *wrapped;

    if (event->type == JW_EVENT_QUIT) {
        jw_context_stop(ctx);
        return;
    }

    if (event->type != JW_EVENT_MOUSE_MOVE) {
        return;
    }

    fill_pattern(pixels, DEMO_WIDTH, DEMO_HEIGHT, event->data.mouse_move.x, event->data.mouse_move.y);
    wrapped = jw_buffer_wrap_pixels(DEMO_WIDTH,
                                    DEMO_HEIGHT,
                                    DEMO_WIDTH * (int)sizeof(uint32_t),
                                    JW_PIXEL_FORMAT_BGRA8888,
                                    pixels,
                                    NULL,
                                    NULL);
    if (!wrapped) {
        return;
    }

    jw_display_present_buffer(display, wrapped);
    jw_buffer_destroy(wrapped);
}

int main(void)
{
    jw_context_t *ctx;
    jw_proxy_t *proxy;
    jw_display_t *display;
    jw_event_manager_t *mouse;
    uint32_t *pixels;
    jw_buffer_t *wrapped;

    pixels = (uint32_t *)calloc((size_t)DEMO_WIDTH * (size_t)DEMO_HEIGHT, sizeof(uint32_t));
    if (!pixels) {
        return 1;
    }

    ctx = jw_context_create();
    proxy = jw_proxy_create_sdl(DEMO_WIDTH, DEMO_HEIGHT, "JingWei external buffer demo");
    display = jw_display_create(DEMO_WIDTH, DEMO_HEIGHT, proxy);
    mouse = jw_event_manager_create_mouse(proxy);
    if (!ctx || !proxy || !display || !mouse || jw_display_bind_event_manager(display, mouse) != 0) {
        fprintf(stderr, "failed to initialize demo\n");
        jw_event_manager_destroy(mouse);
        if (display) {
            jw_display_destroy(display);
        } else {
            jw_proxy_destroy(proxy);
        }
        jw_context_destroy(ctx);
        free(pixels);
        return 1;
    }

    jw_context_register_display(ctx, display);
    jw_context_set_event_callback(ctx, handle_event, pixels);

    fill_pattern(pixels, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH / 2, DEMO_HEIGHT / 2);
    wrapped = jw_buffer_wrap_pixels(DEMO_WIDTH,
                                    DEMO_HEIGHT,
                                    DEMO_WIDTH * (int)sizeof(uint32_t),
                                    JW_PIXEL_FORMAT_BGRA8888,
                                    pixels,
                                    NULL,
                                    NULL);
    jw_display_present_buffer(display, wrapped);
    jw_buffer_destroy(wrapped);

    printf("External BGRA buffer demo running. Move the mouse in the window.\n");
    jw_context_run(ctx);

    jw_context_destroy(ctx);
    free(pixels);
    return 0;
}
