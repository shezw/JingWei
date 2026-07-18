#include "jingwei.h"

#include <stdio.h>

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 480
#define BLOCK_SIZE 48

static uint32_t color_from_xy(int x, int y)
{
    uint8_t r = (uint8_t)(40 + (x * 215) / DEMO_WIDTH);
    uint8_t g = (uint8_t)(50 + (y * 205) / DEMO_HEIGHT);
    uint8_t b = (uint8_t)(180 - ((x + y) % 120));

    return (0xffu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void draw_cursor_block(jw_display_t *display, int x, int y)
{
    jw_buffer_t *fb = jw_display_framebuffer(display);
    jw_rect_t block;

    jw_buffer_fill(fb, 0xff101820u);

    block.x = x - BLOCK_SIZE / 2;
    block.y = y - BLOCK_SIZE / 2;
    block.w = BLOCK_SIZE;
    block.h = BLOCK_SIZE;
    jw_buffer_fill_rect(fb, &block, color_from_xy(x, y));
    jw_display_present(display);
}

static void handle_event(jw_context_t *ctx,
                         jw_display_t *display,
                         const jw_event_t *event,
                         void *user_data)
{
    (void)user_data;

    if (event->type == JW_EVENT_MOUSE_MOVE) {
        int x = event->data.mouse_move.x;
        int y = event->data.mouse_move.y;

        printf("mouse: x=%d y=%d dx=%d dy=%d\n",
               x,
               y,
               event->data.mouse_move.dx,
               event->data.mouse_move.dy);
        draw_cursor_block(display, x, y);
    } else if (event->type == JW_EVENT_QUIT) {
        jw_context_stop(ctx);
    }
}

int main(void)
{
    jw_context_t *ctx;
    jw_proxy_t *proxy;
    jw_display_t *display;
    jw_event_manager_t *mouse;

    ctx = jw_context_create();
    if (!ctx) {
        fprintf(stderr, "failed to create JingWei context\n");
        return 1;
    }

    proxy = jw_proxy_create_fbdev(NULL, DEMO_WIDTH, DEMO_HEIGHT, "JingWei fbdev demo");
    if (!proxy) {
        fprintf(stderr, "failed to create fbdev proxy\n");
        jw_context_destroy(ctx);
        return 1;
    }

    display = jw_display_create(DEMO_WIDTH, DEMO_HEIGHT, proxy);
    if (!display) {
        fprintf(stderr, "failed to create display\n");
        jw_proxy_destroy(proxy);
        jw_context_destroy(ctx);
        return 1;
    }

    mouse = jw_event_manager_create_mouse(proxy);
    if (!mouse || jw_display_bind_event_manager(display, mouse) != 0) {
        fprintf(stderr, "failed to bind mouse event manager\n");
        jw_event_manager_destroy(mouse);
        jw_display_destroy(display);
        jw_context_destroy(ctx);
        return 1;
    }

    jw_context_register_display(ctx, display);
    jw_context_set_event_callback(ctx, handle_event, NULL);
    draw_cursor_block(display, DEMO_WIDTH / 2, DEMO_HEIGHT / 2);

    printf("JingWei fbdev demo running. Move the mouse in the display window.\n");
    jw_context_run(ctx);

    jw_context_destroy(ctx);
    return 0;
}
