#ifndef JINGWEI_H
#define JINGWEI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jw_context jw_context_t;
typedef struct jw_display jw_display_t;
typedef struct jw_buffer jw_buffer_t;
typedef struct jw_proxy jw_proxy_t;
typedef struct jw_event_manager jw_event_manager_t;

typedef enum jw_pixel_format {
    JW_PIXEL_FORMAT_INVALID = 0,
    JW_PIXEL_FORMAT_ARGB8888,
    JW_PIXEL_FORMAT_XRGB8888,
    JW_PIXEL_FORMAT_BGRA8888,
    JW_PIXEL_FORMAT_BGRX8888
} jw_pixel_format_t;

typedef struct jw_rect {
    int x;
    int y;
    int w;
    int h;
} jw_rect_t;

typedef enum jw_event_type {
    JW_EVENT_NONE = 0,
    JW_EVENT_MOUSE_MOVE,
    JW_EVENT_MOUSE_KEY,
    JW_EVENT_MOUSE_WHEEL,
    JW_EVENT_MOUSE_LEAVE,
    JW_EVENT_KEY,
    JW_EVENT_TEXT_INPUT,
    JW_EVENT_RESIZE,
    JW_EVENT_FOCUS,
    JW_EVENT_VISIBILITY,
    JW_EVENT_QUIT,
    JW_EVENT_SYSTEM,
    JW_EVENT_VSYNC
} jw_event_type_t;

typedef enum jw_mouse_button {
    JW_MOUSE_LEFT = 1,
    JW_MOUSE_MIDDLE = 2,
    JW_MOUSE_RIGHT = 3
} jw_mouse_button_t;

typedef enum jw_button_state {
    JW_BUTTON_UP = 0,
    JW_BUTTON_DOWN = 1
} jw_button_state_t;

typedef enum jw_key_state {
    JW_KEY_UP = 0,
    JW_KEY_DOWN = 1
} jw_key_state_t;

typedef enum jw_key_modifier {
    JW_KEY_MOD_NONE = 0,
    JW_KEY_MOD_SHIFT = 1 << 0,
    JW_KEY_MOD_CTRL = 1 << 1,
    JW_KEY_MOD_ALT = 1 << 2,
    JW_KEY_MOD_SUPER = 1 << 3,
    JW_KEY_MOD_KEYPAD = 1 << 4
} jw_key_modifier_t;

typedef void (*jw_buffer_release_t)(void *pixels, void *user_data);

typedef struct jw_event {
    jw_event_type_t type;
    uint64_t timestamp_ms;
    union {
        struct {
            int x;
            int y;
            int dx;
            int dy;
            uint32_t buttons;
        } mouse_move;
        struct {
            int button;
            uint32_t buttons;
            int state;
            int x;
            int y;
        } mouse_key;
        struct {
            int x;
            int y;
            int mouse_x;
            int mouse_y;
        } mouse_wheel;
        struct {
            int key;
            int scancode;
            int state;
            int repeat;
            uint32_t modifiers;
        } key;
        struct {
            char text[32];
        } text;
        struct {
            int width;
            int height;
        } resize;
        struct {
            int focused;
        } focus;
        struct {
            int visible;
        } visibility;
    } data;
} jw_event_t;

typedef void (*jw_event_callback_t)(jw_context_t *ctx,
                                    jw_display_t *display,
                                    const jw_event_t *event,
                                    void *user_data);

jw_context_t *jw_context_create(void);
void jw_context_destroy(jw_context_t *ctx);
int jw_context_register_display(jw_context_t *ctx, jw_display_t *display);
void jw_context_set_event_callback(jw_context_t *ctx,
                                   jw_event_callback_t callback,
                                   void *user_data);
int jw_context_poll(jw_context_t *ctx, int timeout_ms);
int jw_context_run(jw_context_t *ctx);
void jw_context_stop(jw_context_t *ctx);

jw_display_t *jw_display_create(int width, int height, jw_proxy_t *proxy);
void jw_display_destroy(jw_display_t *display);
int jw_display_bind_event_manager(jw_display_t *display, jw_event_manager_t *manager);
int jw_display_set_framebuffer(jw_display_t *display, jw_buffer_t *buffer);
int jw_display_present(jw_display_t *display);
int jw_display_present_buffer(jw_display_t *display, jw_buffer_t *buffer);
int jw_display_present_rects(jw_display_t *display, const jw_rect_t *rects, int rect_count);
int jw_display_resize(jw_display_t *display, int width, int height);
int jw_display_width(const jw_display_t *display);
int jw_display_height(const jw_display_t *display);
jw_buffer_t *jw_display_framebuffer(jw_display_t *display);

jw_buffer_t *jw_buffer_create(int width, int height);
jw_buffer_t *jw_buffer_wrap_pixels(int width,
                                   int height,
                                   int stride,
                                   jw_pixel_format_t format,
                                   void *pixels,
                                   jw_buffer_release_t release,
                                   void *user_data);
void jw_buffer_destroy(jw_buffer_t *buffer);
int jw_buffer_width(const jw_buffer_t *buffer);
int jw_buffer_height(const jw_buffer_t *buffer);
int jw_buffer_stride(const jw_buffer_t *buffer);
jw_pixel_format_t jw_buffer_format(const jw_buffer_t *buffer);
void *jw_buffer_data(jw_buffer_t *buffer);
const void *jw_buffer_const_data(const jw_buffer_t *buffer);
int jw_buffer_fill(jw_buffer_t *buffer, uint32_t argb);
int jw_buffer_fill_rect(jw_buffer_t *buffer, const jw_rect_t *rect, uint32_t argb);

jw_event_manager_t *jw_event_manager_create_mouse(jw_proxy_t *proxy);
void jw_event_manager_destroy(jw_event_manager_t *manager);

jw_proxy_t *jw_proxy_create_sdl(int width, int height, const char *title);
jw_proxy_t *jw_proxy_create_fbdev(const char *device, int width, int height, const char *title);
void jw_proxy_destroy(jw_proxy_t *proxy);

#ifdef __cplusplus
}
#endif

#endif
