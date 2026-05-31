#include "jw_internal.h"

#include <stdlib.h>
#include <string.h>

#if defined(JW_HAVE_SDL2)
#include <SDL2/SDL.h>

typedef struct jw_sdl_impl {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    char *title;
    uint32_t texture_format;
    int sdl_started_here;
} jw_sdl_impl_t;

static char *jw_sdl_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        return NULL;
    }

    len = strlen(text) + 1u;
    copy = (char *)malloc(len);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len);
    return copy;
}

static uint32_t jw_sdl_texture_format(jw_pixel_format_t format)
{
    switch (format) {
    case JW_PIXEL_FORMAT_BGRA8888:
        return SDL_PIXELFORMAT_BGRA8888;
    case JW_PIXEL_FORMAT_BGRX8888:
        return SDL_PIXELFORMAT_BGRX8888;
    case JW_PIXEL_FORMAT_XRGB8888:
        return SDL_PIXELFORMAT_ARGB8888;
    case JW_PIXEL_FORMAT_ARGB8888:
    case JW_PIXEL_FORMAT_INVALID:
    default:
        return SDL_PIXELFORMAT_ARGB8888;
    }
}

static uint32_t jw_sdl_mouse_buttons(uint32_t state)
{
    uint32_t buttons = 0;

    if (state & SDL_BUTTON_LMASK) {
        buttons |= 1u << JW_MOUSE_LEFT;
    }
    if (state & SDL_BUTTON_MMASK) {
        buttons |= 1u << JW_MOUSE_MIDDLE;
    }
    if (state & SDL_BUTTON_RMASK) {
        buttons |= 1u << JW_MOUSE_RIGHT;
    }
    return buttons;
}

static uint32_t jw_sdl_key_modifiers(SDL_Keymod mod)
{
    uint32_t result = JW_KEY_MOD_NONE;

    if (mod & KMOD_SHIFT) {
        result |= JW_KEY_MOD_SHIFT;
    }
    if (mod & KMOD_CTRL) {
        result |= JW_KEY_MOD_CTRL;
    }
    if (mod & KMOD_ALT) {
        result |= JW_KEY_MOD_ALT;
    }
    if (mod & KMOD_GUI) {
        result |= JW_KEY_MOD_SUPER;
    }
    if (mod & KMOD_NUM) {
        result |= JW_KEY_MOD_KEYPAD;
    }
    return result;
}

static int jw_sdl_recreate_texture(jw_proxy_t *proxy, jw_pixel_format_t format)
{
    jw_sdl_impl_t *impl = (jw_sdl_impl_t *)proxy->impl;
    uint32_t texture_format = jw_sdl_texture_format(format);

    if (impl->texture && impl->texture_format == texture_format) {
        return 0;
    }

    if (impl->texture) {
        SDL_DestroyTexture(impl->texture);
        impl->texture = NULL;
    }

    impl->texture = SDL_CreateTexture(impl->renderer,
                                      texture_format,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      proxy->width,
                                      proxy->height);
    if (!impl->texture) {
        return -1;
    }
    impl->texture_format = texture_format;
    return 0;
}

static int jw_sdl_init(jw_proxy_t *proxy)
{
    jw_sdl_impl_t *impl = (jw_sdl_impl_t *)proxy->impl;

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            return -1;
        }
        impl->sdl_started_here = 1;
    }

    impl->window = SDL_CreateWindow(impl->title ? impl->title : "JingWei SDL Display",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    proxy->width,
                                    proxy->height,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!impl->window) {
        return -1;
    }

    impl->renderer = SDL_CreateRenderer(impl->window, -1, SDL_RENDERER_ACCELERATED);
    if (!impl->renderer) {
        impl->renderer = SDL_CreateRenderer(impl->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!impl->renderer) {
        return -1;
    }

    if (jw_sdl_recreate_texture(proxy, JW_PIXEL_FORMAT_ARGB8888) != 0) {
        return -1;
    }
    SDL_StartTextInput();

    return 0;
}

static void jw_sdl_deinit(jw_proxy_t *proxy)
{
    jw_sdl_impl_t *impl;

    if (!proxy || !proxy->impl) {
        return;
    }

    impl = (jw_sdl_impl_t *)proxy->impl;
    SDL_StopTextInput();
    if (impl->texture) {
        SDL_DestroyTexture(impl->texture);
        impl->texture = NULL;
    }
    if (impl->renderer) {
        SDL_DestroyRenderer(impl->renderer);
        impl->renderer = NULL;
    }
    if (impl->window) {
        SDL_DestroyWindow(impl->window);
        impl->window = NULL;
    }
    if (impl->sdl_started_here) {
        SDL_Quit();
    }
    free(impl->title);
    impl->title = NULL;
}

static int jw_sdl_commit(jw_proxy_t *proxy, const jw_buffer_t *buffer)
{
    jw_sdl_impl_t *impl;

    if (!proxy || !proxy->impl || !buffer || !buffer->pixels) {
        return -1;
    }

    impl = (jw_sdl_impl_t *)proxy->impl;
    if (!impl->renderer) {
        return -1;
    }

    if (proxy->width != buffer->width || proxy->height != buffer->height) {
        proxy->width = buffer->width;
        proxy->height = buffer->height;
    }
    if (jw_sdl_recreate_texture(proxy, buffer->format) != 0) {
        return -1;
    }
    if (SDL_UpdateTexture(impl->texture, NULL, buffer->pixels, buffer->stride) != 0) {
        return -1;
    }

    SDL_RenderClear(impl->renderer);
    SDL_RenderCopy(impl->renderer, impl->texture, NULL, NULL);
    SDL_RenderPresent(impl->renderer);
    return 0;
}

static int jw_sdl_commit_rects(jw_proxy_t *proxy, const jw_buffer_t *buffer, const jw_rect_t *rects, int rect_count)
{
    jw_sdl_impl_t *impl;
    int i;

    if (!proxy || !proxy->impl || !buffer || !buffer->pixels || rect_count <= 0 || !rects) {
        return jw_sdl_commit(proxy, buffer);
    }

    impl = (jw_sdl_impl_t *)proxy->impl;
    if (proxy->width != buffer->width || proxy->height != buffer->height) {
        proxy->width = buffer->width;
        proxy->height = buffer->height;
    }
    if (!impl->renderer || jw_sdl_recreate_texture(proxy, buffer->format) != 0) {
        return -1;
    }

    for (i = 0; i < rect_count; ++i) {
        SDL_Rect rect;
        const unsigned char *pixels;
        int x0 = rects[i].x;
        int y0 = rects[i].y;
        int x1 = rects[i].x + rects[i].w;
        int y1 = rects[i].y + rects[i].h;

        if (x0 < 0) {
            x0 = 0;
        }
        if (y0 < 0) {
            y0 = 0;
        }
        if (x1 > buffer->width) {
            x1 = buffer->width;
        }
        if (y1 > buffer->height) {
            y1 = buffer->height;
        }
        if (x0 >= x1 || y0 >= y1) {
            continue;
        }

        rect.x = x0;
        rect.y = y0;
        rect.w = x1 - x0;
        rect.h = y1 - y0;
        pixels = (const unsigned char *)buffer->pixels + ((size_t)rect.y * (size_t)buffer->stride) + ((size_t)rect.x * sizeof(uint32_t));
        if (SDL_UpdateTexture(impl->texture, &rect, pixels, buffer->stride) != 0) {
            return -1;
        }
    }

    SDL_RenderClear(impl->renderer);
    SDL_RenderCopy(impl->renderer, impl->texture, NULL, NULL);
    SDL_RenderPresent(impl->renderer);
    return 0;
}

static int jw_sdl_resize(jw_proxy_t *proxy, int width, int height)
{
    jw_sdl_impl_t *impl;

    if (!proxy || !proxy->impl || width <= 0 || height <= 0) {
        return -1;
    }

    impl = (jw_sdl_impl_t *)proxy->impl;
    proxy->width = width;
    proxy->height = height;
    if (impl->window) {
        SDL_SetWindowSize(impl->window, width, height);
    }
    if (impl->texture) {
        SDL_DestroyTexture(impl->texture);
        impl->texture = NULL;
    }
    return jw_sdl_recreate_texture(proxy, JW_PIXEL_FORMAT_ARGB8888);
}

static int jw_sdl_poll_event(jw_proxy_t *proxy, jw_event_t *event)
{
    SDL_Event sdl_event;

    if (!event || SDL_PollEvent(&sdl_event) == 0) {
        return 0;
    }

    memset(event, 0, sizeof(*event));
    event->timestamp_ms = jw_time_now_ms();

    switch (sdl_event.type) {
    case SDL_QUIT:
        event->type = JW_EVENT_QUIT;
        return 1;
    case SDL_MOUSEMOTION:
        event->type = JW_EVENT_MOUSE_MOVE;
        event->data.mouse_move.x = sdl_event.motion.x;
        event->data.mouse_move.y = sdl_event.motion.y;
        event->data.mouse_move.dx = sdl_event.motion.xrel;
        event->data.mouse_move.dy = sdl_event.motion.yrel;
        event->data.mouse_move.buttons = jw_sdl_mouse_buttons(sdl_event.motion.state);
        return 1;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        event->type = JW_EVENT_MOUSE_KEY;
        event->data.mouse_key.button = sdl_event.button.button;
        event->data.mouse_key.buttons = jw_sdl_mouse_buttons(SDL_GetMouseState(NULL, NULL));
        event->data.mouse_key.state = sdl_event.type == SDL_MOUSEBUTTONDOWN ? JW_BUTTON_DOWN : JW_BUTTON_UP;
        event->data.mouse_key.x = sdl_event.button.x;
        event->data.mouse_key.y = sdl_event.button.y;
        return 1;
    case SDL_WINDOWEVENT:
        if (sdl_event.window.event == SDL_WINDOWEVENT_LEAVE) {
            event->type = JW_EVENT_MOUSE_LEAVE;
            return 1;
        }
        if (sdl_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            event->type = JW_EVENT_RESIZE;
            event->data.resize.width = sdl_event.window.data1;
            event->data.resize.height = sdl_event.window.data2;
            if (proxy) {
                proxy->width = event->data.resize.width;
                proxy->height = event->data.resize.height;
            }
            return 1;
        }
        if (sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
            sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            event->type = JW_EVENT_FOCUS;
            event->data.focus.focused = sdl_event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
            return 1;
        }
        if (sdl_event.window.event == SDL_WINDOWEVENT_SHOWN ||
            sdl_event.window.event == SDL_WINDOWEVENT_HIDDEN) {
            event->type = JW_EVENT_VISIBILITY;
            event->data.visibility.visible = sdl_event.window.event == SDL_WINDOWEVENT_SHOWN;
            return 1;
        }
        return 0;
    case SDL_MOUSEWHEEL:
        event->type = JW_EVENT_MOUSE_WHEEL;
        event->data.mouse_wheel.x = sdl_event.wheel.x;
        event->data.mouse_wheel.y = sdl_event.wheel.y;
        SDL_GetMouseState(&event->data.mouse_wheel.mouse_x, &event->data.mouse_wheel.mouse_y);
        return 1;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        event->type = JW_EVENT_KEY;
        event->data.key.key = sdl_event.key.keysym.sym;
        event->data.key.scancode = sdl_event.key.keysym.scancode;
        event->data.key.state = sdl_event.type == SDL_KEYDOWN ? JW_KEY_DOWN : JW_KEY_UP;
        event->data.key.repeat = sdl_event.key.repeat != 0;
        event->data.key.modifiers = jw_sdl_key_modifiers(sdl_event.key.keysym.mod);
        return 1;
    case SDL_TEXTINPUT:
        event->type = JW_EVENT_TEXT_INPUT;
        strncpy(event->data.text.text, sdl_event.text.text, sizeof(event->data.text.text) - 1u);
        return 1;
    default:
        return 0;
    }
}

static const jw_proxy_ops_t JW_SDL_OPS = {
    jw_sdl_init,
    jw_sdl_deinit,
    jw_sdl_commit,
    jw_sdl_commit_rects,
    jw_sdl_resize,
    jw_sdl_poll_event
};

jw_proxy_t *jw_proxy_create_sdl(int width, int height, const char *title)
{
    jw_sdl_impl_t *impl;

    if (width <= 0 || height <= 0) {
        return NULL;
    }

    impl = (jw_sdl_impl_t *)calloc(1, sizeof(*impl));
    if (!impl) {
        return NULL;
    }

    if (title) {
        impl->title = jw_sdl_strdup(title);
    }

    return jw_proxy_alloc(&JW_SDL_OPS, width, height, impl);
}

#else

jw_proxy_t *jw_proxy_create_sdl(int width, int height, const char *title)
{
    (void)width;
    (void)height;
    (void)title;
    return NULL;
}

#endif
