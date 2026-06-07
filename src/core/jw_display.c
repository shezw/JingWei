#include "jw_internal.h"

#include <stdlib.h>

jw_display_t *jw_display_create(int width, int height, jw_proxy_t *proxy)
{
    jw_display_t *display;

    if (width <= 0 || height <= 0 || !proxy) {
        return NULL;
    }

    display = (jw_display_t *)calloc(1, sizeof(*display));
    if (!display) {
        return NULL;
    }

    display->framebuffer = jw_buffer_create(width, height);
    if (!display->framebuffer) {
        free(display);
        return NULL;
    }

    display->width = width;
    display->height = height;
    display->owns_framebuffer = 1;
    display->proxy = proxy;

    if (proxy->ops && proxy->ops->init && proxy->ops->init(proxy) != 0) {
        if (proxy->ops->deinit) {
            proxy->ops->deinit(proxy);
        }
        jw_buffer_destroy(display->framebuffer);
        free(display);
        return NULL;
    }

    if (proxy->width > 0 && proxy->height > 0 &&
        (proxy->width != display->width || proxy->height != display->height)) {
        jw_buffer_t *framebuffer = jw_buffer_create(proxy->width, proxy->height);
        if (!framebuffer) {
            if (proxy->ops && proxy->ops->deinit) {
                proxy->ops->deinit(proxy);
            }
            jw_buffer_destroy(display->framebuffer);
            free(display);
            return NULL;
        }
        jw_buffer_destroy(display->framebuffer);
        display->framebuffer = framebuffer;
        display->width = proxy->width;
        display->height = proxy->height;
    }

    return display;
}

void jw_display_destroy(jw_display_t *display)
{
    if (!display) {
        return;
    }

    jw_event_manager_destroy(display->event_manager);
    if (display->proxy && display->proxy->ops && display->proxy->ops->deinit) {
        display->proxy->ops->deinit(display->proxy);
    }
    jw_proxy_destroy(display->proxy);
    if (display->owns_framebuffer) {
        jw_buffer_destroy(display->framebuffer);
    }
    free(display);
}

int jw_display_bind_event_manager(jw_display_t *display, jw_event_manager_t *manager)
{
    if (!display || !manager) {
        return -1;
    }

    display->event_manager = manager;
    return 0;
}

int jw_display_set_framebuffer(jw_display_t *display, jw_buffer_t *buffer)
{
    if (!display || !buffer) {
        return -1;
    }

    if (display->owns_framebuffer) {
        jw_buffer_destroy(display->framebuffer);
    }
    display->framebuffer = buffer;
    display->width = jw_buffer_width(buffer);
    display->height = jw_buffer_height(buffer);
    display->owns_framebuffer = 0;
    return 0;
}

int jw_display_present(jw_display_t *display)
{
    if (!display || !display->proxy || !display->proxy->ops || !display->proxy->ops->commit) {
        return -1;
    }

    return display->proxy->ops->commit(display->proxy, display->framebuffer);
}

int jw_display_present_buffer(jw_display_t *display, jw_buffer_t *buffer)
{
    if (!display || !buffer || !display->proxy || !display->proxy->ops || !display->proxy->ops->commit) {
        return -1;
    }

    return display->proxy->ops->commit(display->proxy, buffer);
}

int jw_display_present_rects(jw_display_t *display, const jw_rect_t *rects, int rect_count)
{
    if (!display || !display->proxy || !display->proxy->ops || !display->framebuffer) {
        return -1;
    }

    if (display->proxy->ops->commit_rects) {
        return display->proxy->ops->commit_rects(display->proxy, display->framebuffer, rects, rect_count);
    }
    return jw_display_present(display);
}

int jw_display_resize(jw_display_t *display, int width, int height)
{
    jw_buffer_t *buffer;

    if (!display || width <= 0 || height <= 0) {
        return -1;
    }

    if (display->proxy && display->proxy->ops && display->proxy->ops->resize) {
        if (display->proxy->ops->resize(display->proxy, width, height) != 0) {
            return -1;
        }
    }

    display->width = width;
    display->height = height;
    if (!display->owns_framebuffer) {
        return 0;
    }

    buffer = jw_buffer_create(width, height);
    if (!buffer) {
        return -1;
    }
    jw_buffer_destroy(display->framebuffer);
    display->framebuffer = buffer;
    return 0;
}

int jw_display_width(const jw_display_t *display)
{
    return display ? display->width : 0;
}

int jw_display_height(const jw_display_t *display)
{
    return display ? display->height : 0;
}

jw_buffer_t *jw_display_framebuffer(jw_display_t *display)
{
    return display ? display->framebuffer : NULL;
}
