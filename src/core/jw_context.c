#include "jw_internal.h"

#include <stdlib.h>

jw_context_t *jw_context_create(void)
{
    return (jw_context_t *)calloc(1, sizeof(jw_context_t));
}

void jw_context_destroy(jw_context_t *ctx)
{
    int i;

    if (!ctx) {
        return;
    }

    for (i = 0; i < ctx->display_count; ++i) {
        jw_display_destroy(ctx->displays[i]);
    }
    free(ctx->displays);
    free(ctx);
}

int jw_context_register_display(jw_context_t *ctx, jw_display_t *display)
{
    jw_display_t **next;
    int next_capacity;

    if (!ctx || !display) {
        return -1;
    }

    if (ctx->display_count == ctx->display_capacity) {
        next_capacity = ctx->display_capacity == 0 ? 2 : ctx->display_capacity * 2;
        next = (jw_display_t **)realloc(ctx->displays, (size_t)next_capacity * sizeof(*next));
        if (!next) {
            return -1;
        }
        ctx->displays = next;
        ctx->display_capacity = next_capacity;
    }

    display->id = ctx->display_count + 1;
    ctx->displays[ctx->display_count++] = display;
    return display->id;
}

void jw_context_set_event_callback(jw_context_t *ctx,
                                   jw_event_callback_t callback,
                                   void *user_data)
{
    if (!ctx) {
        return;
    }

    ctx->event_callback = callback;
    ctx->event_user_data = user_data;
}

int jw_context_poll(jw_context_t *ctx, int timeout_ms)
{
    int i;
    int event_count = 0;

    if (!ctx) {
        return -1;
    }

    for (i = 0; i < ctx->display_count; ++i) {
        jw_display_t *display = ctx->displays[i];
        jw_event_t event;

        while (display && display->event_manager &&
               jw_event_manager_poll(display->event_manager, &event) > 0) {
            ++event_count;
            if (event.type == JW_EVENT_QUIT) {
                ctx->running = 0;
            }
            if (event.type == JW_EVENT_RESIZE) {
                jw_display_resize(display, event.data.resize.width, event.data.resize.height);
            }
            if (ctx->event_callback) {
                ctx->event_callback(ctx, display, &event, ctx->event_user_data);
            }
        }
    }

    if (event_count == 0 && timeout_ms > 0) {
        jw_sleep_ms((unsigned int)timeout_ms);
    }

    return event_count;
}

int jw_context_run(jw_context_t *ctx)
{
    int i;

    if (!ctx) {
        return -1;
    }

    ctx->running = 1;
    while (ctx->running) {
        if (jw_context_poll(ctx, 0) == 0) {
            for (i = 0; i < ctx->display_count; ++i) {
                jw_display_present(ctx->displays[i]);
            }
            jw_sleep_ms(10);
        }
    }

    return 0;
}

void jw_context_stop(jw_context_t *ctx)
{
    if (ctx) {
        ctx->running = 0;
    }
}
