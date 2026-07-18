/*
    JingWei
    apps/jw-wpe-browser main.c    2026-07-18

     ______     __  __     ______     ______     __     __
    /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
    \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
     \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
      \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com

    @link    : https://github.com/shezw/JingWei
    @author  : shezw
    @email   : hello@shezw.com
*/

#include "jw_wpe.h"
#include "jw_wpe_input.h"

#include <errno.h>
#include <glib-unix.h>
#include <jingwei/vnc.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wpe/webkit.h>

#define JW_BROWSER_WIDTH 1280U
#define JW_BROWSER_HEIGHT 720U
#define JW_BROWSER_DEFAULT_VNC_PORT 5900
#define JW_BROWSER_INPUT_INTERVAL_MS 8U

#ifndef JW_BROWSER_DEFAULT_URL
#define JW_BROWSER_DEFAULT_URL \
    "https://shezw.github.io/cube3d/demo/index.html"
#endif

typedef struct jw_browser_state {
    GMainLoop *main_loop;
    jw_context_t *context;
    jw_display_t *platform_display;
    WPEDisplay *display;
    WebKitWebView *web_view;
    WPEView *wpe_view;
    guint context_source_id;
    guint signal_int_source_id;
    guint signal_term_source_id;
    gint pointer_x;
    gint pointer_y;
    guint32 pointer_buttons;
    jw_wpe_modifier_state_t keyboard_modifiers;
    guint64 input_reset_count;
    int vnc_port;
    gboolean platform_display_registered;
    gboolean pointer_inside;
    gboolean load_finished;
    gboolean first_frame_received;
    gboolean ready_printed;
    gboolean runtime_failed;
} jw_browser_state_t;

static void jw_browser_maybe_ready(jw_browser_state_t *state)
{
    const char *current_uri;

    if (state->ready_printed || !state->load_finished ||
        !state->first_frame_received) {
        return;
    }

    current_uri = webkit_web_view_get_uri(state->web_view);
    state->ready_printed = TRUE;
    g_print(
        "JW_WPE_READY url=%s size=1280x720 vnc_port=%d frames=%"
        G_GUINT64_FORMAT "\n",
        current_uri != NULL ? current_uri : "",
        state->vnc_port,
        jw_wpe_display_get_frame_count(state->display));
    fflush(stdout);
}

static void jw_browser_first_frame(
    WPEDisplay *display,
    gpointer user_data)
{
    jw_browser_state_t *state = user_data;

    (void)display;
    state->first_frame_received = TRUE;
    jw_browser_maybe_ready(state);
}

static gboolean jw_browser_load_failed(
    WebKitWebView *web_view,
    WebKitLoadEvent load_event,
    const char *failing_uri,
    GError *error,
    gpointer user_data)
{
    (void)web_view;
    (void)user_data;
    g_printerr(
        "JW_WPE_LOAD_FAILED event=%d uri=%s error=%s\n",
        load_event,
        failing_uri != NULL ? failing_uri : "",
        error != NULL ? error->message : "unknown error");
    return FALSE;
}

static void jw_browser_load_changed(
    WebKitWebView *web_view,
    WebKitLoadEvent load_event,
    gpointer user_data)
{
    jw_browser_state_t *state = user_data;

    (void)web_view;
    if (load_event != WEBKIT_LOAD_FINISHED) {
        return;
    }
    state->load_finished = TRUE;
    jw_browser_maybe_ready(state);
}

static guint32 jw_browser_event_time(const jw_event_t *event)
{
    if (event != NULL && event->timestamp_ms != 0) {
        return (guint32)event->timestamp_ms;
    }
    return (guint32)(g_get_monotonic_time() / 1000);
}

static void jw_browser_clear_secret(char *secret, gsize length)
{
    volatile guint8 *bytes = (volatile guint8 *)secret;

    if (secret == NULL) {
        return;
    }
    while (length > 0) {
        bytes[--length] = 0;
    }
}

static gboolean jw_browser_load_vnc_password(
    const char *path,
    char **password,
    gsize *password_length)
{
    char *contents = NULL;
    gsize length = 0;
    gsize index;

    if (path == NULL || path[0] == '\0' || password == NULL ||
        password_length == NULL ||
        !g_file_get_contents(path, &contents, &length, NULL)) {
        return FALSE;
    }
    if (length > 0 && contents[length - 1] == '\n') {
        contents[--length] = '\0';
        if (length > 0 && contents[length - 1] == '\r') {
            contents[--length] = '\0';
        }
    }
    if (length == 0 || length > 8) {
        jw_browser_clear_secret(contents, length);
        g_free(contents);
        return FALSE;
    }
    for (index = 0; index < length; ++index) {
        guint8 value = (guint8)contents[index];

        if (value < 0x21U || value > 0x7eU) {
            jw_browser_clear_secret(contents, length);
            g_free(contents);
            return FALSE;
        }
    }
    *password = contents;
    *password_length = length;
    return TRUE;
}

static WPEModifiers jw_browser_modifiers(
    const jw_browser_state_t *state,
    guint32 buttons)
{
    guint modifiers = jw_wpe_modifier_state_get(&state->keyboard_modifiers);

    if ((buttons & (1U << JW_MOUSE_LEFT)) != 0) {
        modifiers |= WPE_MODIFIER_POINTER_BUTTON1;
    }
    if ((buttons & (1U << JW_MOUSE_MIDDLE)) != 0) {
        modifiers |= WPE_MODIFIER_POINTER_BUTTON2;
    }
    if ((buttons & (1U << JW_MOUSE_RIGHT)) != 0) {
        modifiers |= WPE_MODIFIER_POINTER_BUTTON3;
    }
    return (WPEModifiers)modifiers;
}

static void jw_browser_send_event(
    jw_browser_state_t *state,
    WPEEvent *event)
{
    if (event == NULL) {
        return;
    }
    wpe_view_event(state->wpe_view, event);
    wpe_event_unref(event);
}

static void jw_browser_send_pointer_move(
    jw_browser_state_t *state,
    const jw_event_t *event)
{
    guint32 time = jw_browser_event_time(event);

    if (!state->pointer_inside) {
        jw_browser_send_event(
            state,
            wpe_event_pointer_move_new(
                WPE_EVENT_POINTER_ENTER,
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, event->data.mouse_move.buttons),
                event->data.mouse_move.x,
                event->data.mouse_move.y,
                0,
                0));
        state->pointer_inside = TRUE;
    } else {
        jw_browser_send_event(
            state,
            wpe_event_pointer_move_new(
                WPE_EVENT_POINTER_MOVE,
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, event->data.mouse_move.buttons),
                event->data.mouse_move.x,
                event->data.mouse_move.y,
                event->data.mouse_move.dx,
                event->data.mouse_move.dy));
    }

    state->pointer_x = event->data.mouse_move.x;
    state->pointer_y = event->data.mouse_move.y;
    state->pointer_buttons = event->data.mouse_move.buttons;
}

static guint jw_browser_wpe_button(int button)
{
    switch (button) {
    case JW_MOUSE_LEFT:
        return WPE_BUTTON_PRIMARY;
    case JW_MOUSE_MIDDLE:
        return WPE_BUTTON_MIDDLE;
    case JW_MOUSE_RIGHT:
        return WPE_BUTTON_SECONDARY;
    default:
        return 0;
    }
}

static void jw_browser_send_pointer_button(
    jw_browser_state_t *state,
    const jw_event_t *event)
{
    gboolean pressed = event->data.mouse_key.state == JW_BUTTON_DOWN;
    guint32 time = jw_browser_event_time(event);
    guint button = jw_browser_wpe_button(event->data.mouse_key.button);
    guint press_count;

    if (button == 0) {
        return;
    }
    press_count = pressed
        ? wpe_view_compute_press_count(
            state->wpe_view,
            event->data.mouse_key.x,
            event->data.mouse_key.y,
            button,
            time)
        : 0;
    jw_browser_send_event(
        state,
        wpe_event_pointer_button_new(
            pressed ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP,
            state->wpe_view,
            WPE_INPUT_SOURCE_MOUSE,
            time,
            jw_browser_modifiers(state, event->data.mouse_key.buttons),
            button,
            event->data.mouse_key.x,
            event->data.mouse_key.y,
            press_count));
    state->pointer_x = event->data.mouse_key.x;
    state->pointer_y = event->data.mouse_key.y;
    state->pointer_buttons = event->data.mouse_key.buttons;
}

static void jw_browser_send_pointer_wheel(
    jw_browser_state_t *state,
    const jw_event_t *event)
{
    jw_browser_send_event(
        state,
        wpe_event_scroll_new(
            state->wpe_view,
            WPE_INPUT_SOURCE_MOUSE,
            jw_browser_event_time(event),
            jw_browser_modifiers(state, state->pointer_buttons),
            event->data.mouse_wheel.x,
            event->data.mouse_wheel.y,
            FALSE,
            FALSE,
            event->data.mouse_wheel.mouse_x,
            event->data.mouse_wheel.mouse_y));
    state->pointer_x = event->data.mouse_wheel.mouse_x;
    state->pointer_y = event->data.mouse_wheel.mouse_y;
}

static void jw_browser_send_key_event(
    jw_browser_state_t *state,
    const jw_event_t *event)
{
    gboolean pressed = event->data.key.state == JW_KEY_DOWN;

    jw_wpe_modifier_state_update(
        &state->keyboard_modifiers,
        (guint)event->data.key.key,
        pressed);
    jw_browser_send_event(
        state,
        wpe_event_keyboard_new(
            pressed
                ? WPE_EVENT_KEYBOARD_KEY_DOWN
                : WPE_EVENT_KEYBOARD_KEY_UP,
            state->wpe_view,
            WPE_INPUT_SOURCE_KEYBOARD,
            jw_browser_event_time(event),
            jw_browser_modifiers(state, state->pointer_buttons),
            (guint)event->data.key.scancode,
            (guint)event->data.key.key));
}

static void jw_browser_reset_input(
    jw_browser_state_t *state,
    uint64_t timestamp_ms)
{
    wpe_view_focus_out(state->wpe_view);
    state->pointer_buttons = 0;
    jw_wpe_modifier_state_reset(&state->keyboard_modifiers);
    state->input_reset_count += 1;
    wpe_view_focus_in(state->wpe_view);

    g_printerr(
        "JW_WPE_INPUT_RESET count=%" G_GUINT64_FORMAT
        " timestamp_ms=%" G_GUINT64_FORMAT "\n",
        state->input_reset_count,
        (guint64)timestamp_ms);
    fflush(stderr);
}

static void jw_browser_handle_event(
    jw_context_t *context,
    jw_display_t *display,
    const jw_event_t *event,
    void *user_data)
{
    jw_browser_state_t *state = user_data;

    (void)context;
    if (display != state->platform_display || state->wpe_view == NULL) {
        return;
    }
    switch (event->type) {
    case JW_EVENT_MOUSE_MOVE:
        jw_browser_send_pointer_move(state, event);
        break;
    case JW_EVENT_MOUSE_KEY:
        jw_browser_send_pointer_button(state, event);
        break;
    case JW_EVENT_MOUSE_WHEEL:
        jw_browser_send_pointer_wheel(state, event);
        break;
    case JW_EVENT_KEY:
        jw_browser_send_key_event(state, event);
        break;
    case JW_EVENT_INPUT_RESET:
        jw_browser_reset_input(state, event->timestamp_ms);
        break;
    default:
        break;
    }
}

static gboolean jw_browser_poll_context(gpointer user_data)
{
    jw_browser_state_t *state = user_data;

    if (jw_context_poll(state->context, 0) < 0) {
        g_printerr("JingWei context event polling failed\n");
        state->runtime_failed = TRUE;
        g_main_loop_quit(state->main_loop);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean jw_browser_quit(gpointer user_data)
{
    jw_browser_state_t *state = user_data;

    g_main_loop_quit(state->main_loop);
    return G_SOURCE_REMOVE;
}

static void jw_browser_web_process_terminated(
    WebKitWebView *web_view,
    WebKitWebProcessTerminationReason reason,
    gpointer user_data)
{
    jw_browser_state_t *state = user_data;

    (void)web_view;
    g_printerr("WPE web process terminated: %d\n", reason);
    state->runtime_failed = TRUE;
    g_main_loop_quit(state->main_loop);
}

static gboolean jw_browser_parse_port(const char *value, int *port)
{
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        *port = JW_BROWSER_DEFAULT_VNC_PORT;
        return TRUE;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        parsed < 1 || parsed > 65535) {
        return FALSE;
    }
    *port = (int)parsed;
    return TRUE;
}

static gboolean jw_browser_parse_cache_model(
    const char *value,
    WebKitCacheModel *cache_model)
{
    if (value == NULL || value[0] == '\0' ||
        g_str_equal(value, "document-viewer")) {
        *cache_model = WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER;
        return TRUE;
    }
    if (g_str_equal(value, "document-browser")) {
        *cache_model = WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER;
        return TRUE;
    }
    if (g_str_equal(value, "web-browser")) {
        *cache_model = WEBKIT_CACHE_MODEL_WEB_BROWSER;
        return TRUE;
    }
    return FALSE;
}

static void jw_browser_remove_source(guint *source_id)
{
    if (*source_id != 0 &&
        g_main_context_find_source_by_id(
            g_main_context_default(),
            *source_id) != NULL) {
        g_source_remove(*source_id);
    }
    *source_id = 0;
}

static void jw_browser_cleanup(jw_browser_state_t *state)
{
    jw_browser_remove_source(&state->context_source_id);
    jw_browser_remove_source(&state->signal_int_source_id);
    jw_browser_remove_source(&state->signal_term_source_id);
    g_clear_object(&state->web_view);
    state->wpe_view = NULL;
    g_clear_object(&state->display);
    if (state->platform_display_registered) {
        jw_context_destroy(state->context);
        state->context = NULL;
        state->platform_display = NULL;
        state->platform_display_registered = FALSE;
    } else {
        jw_display_destroy(state->platform_display);
        state->platform_display = NULL;
        jw_context_destroy(state->context);
        state->context = NULL;
    }
    if (state->main_loop != NULL) {
        g_main_loop_unref(state->main_loop);
    }
}

int main(int argc, char **argv)
{
    const char *environment_url = g_getenv("JINGWEI_BROWSER_URL");
    const char *url = argc > 1
        ? argv[1]
        : environment_url != NULL && environment_url[0] != '\0'
            ? environment_url
            : JW_BROWSER_DEFAULT_URL;
    const char *port_value = g_getenv("VNC_PORT");
    const char *cache_model_value = g_getenv("JINGWEI_CACHE_MODEL");
    const char *password_file = g_getenv("JINGWEI_VNC_PASSWORD_FILE");
    jw_browser_state_t state = { 0 };
    jw_vnc_config_t vnc_config = { 0 };
    jw_proxy_t *proxy = NULL;
    jw_event_manager_t *event_manager = NULL;
    WebKitCacheModel cache_model;
    WPEToplevel *toplevel;
    GError *error = NULL;
    int port;
    int result = EXIT_FAILURE;
    char *vnc_password = NULL;
    gsize vnc_password_length = 0;

    if (argc > 2) {
        g_printerr("Usage: %s [URL]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (!jw_browser_parse_port(port_value, &port)) {
        g_printerr("Invalid VNC_PORT: %s\n", port_value);
        return EXIT_FAILURE;
    }
    if (!jw_browser_parse_cache_model(cache_model_value, &cache_model)) {
        g_printerr(
            "Invalid JINGWEI_CACHE_MODEL: %s "
            "(expected document-viewer, document-browser, or web-browser)\n",
            cache_model_value);
        return EXIT_FAILURE;
    }
    if (!jw_vnc_backend_is_available()) {
        g_printerr("JingWei was built without LibVNCServer support\n");
        return EXIT_FAILURE;
    }
    if (!jw_browser_load_vnc_password(
            password_file,
            &vnc_password,
            &vnc_password_length)) {
        g_printerr(
            "JINGWEI_VNC_PASSWORD_FILE must contain 1..8 printable "
            "ASCII bytes\n");
        return EXIT_FAILURE;
    }

    state.vnc_port = port;
    state.main_loop = g_main_loop_new(NULL, FALSE);
    state.context = jw_context_create();
    if (state.main_loop == NULL || state.context == NULL) {
        g_printerr("Failed to create the JingWei browser context\n");
        goto cleanup;
    }

    vnc_config.port = port;
    vnc_config.desktop_name = "JingWei WPE Browser";
    vnc_config.password = vnc_password;
    proxy = jw_proxy_create_vnc(
        (int)JW_BROWSER_WIDTH,
        (int)JW_BROWSER_HEIGHT,
        &vnc_config);
    jw_browser_clear_secret(vnc_password, vnc_password_length);
    g_clear_pointer(&vnc_password, g_free);
    vnc_password_length = 0;
    if (proxy == NULL) {
        g_printerr("Failed to create the JingWei VNC proxy\n");
        goto cleanup;
    }

    state.platform_display = jw_display_create(
        (int)JW_BROWSER_WIDTH,
        (int)JW_BROWSER_HEIGHT,
        proxy);
    if (state.platform_display == NULL) {
        g_printerr("Failed to create the JingWei platform display\n");
        goto cleanup;
    }
    event_manager = jw_event_manager_create_mouse(proxy);
    proxy = NULL;
    if (event_manager == NULL ||
        jw_display_bind_event_manager(
            state.platform_display, event_manager) != 0) {
        g_printerr("Failed to bind the JingWei event manager\n");
        goto cleanup;
    }
    event_manager = NULL;
    if (jw_context_register_display(
            state.context, state.platform_display) <= 0) {
        g_printerr("Failed to register the JingWei platform display\n");
        goto cleanup;
    }
    state.platform_display_registered = TRUE;
    jw_context_set_event_callback(
        state.context, jw_browser_handle_event, &state);

    state.display = jw_wpe_display_new(state.platform_display);
    if (state.display == NULL ||
        !wpe_display_connect(state.display, &error)) {
        g_printerr(
            "Failed to connect the JingWei WPE display: %s\n",
            error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        goto cleanup;
    }
    g_signal_connect(
        state.display,
        "first-frame",
        G_CALLBACK(jw_browser_first_frame),
        &state);

    webkit_web_context_set_cache_model(
        webkit_web_context_get_default(), cache_model);
    state.web_view = WEBKIT_WEB_VIEW(g_object_new(
        WEBKIT_TYPE_WEB_VIEW,
        "display",
        state.display,
        NULL));
    state.wpe_view = webkit_web_view_get_wpe_view(state.web_view);
    if (state.wpe_view == NULL) {
        g_printerr("WebKit did not create a WPEPlatform view\n");
        goto cleanup;
    }
    toplevel = wpe_view_get_toplevel(state.wpe_view);
    if (toplevel == NULL) {
        WPEToplevel *created_toplevel =
            wpe_display_create_toplevel(state.display, 1);

        if (created_toplevel != NULL) {
            wpe_view_set_toplevel(state.wpe_view, created_toplevel);
            g_object_unref(created_toplevel);
            toplevel = wpe_view_get_toplevel(state.wpe_view);
        }
    }
    if (toplevel == NULL ||
        !wpe_toplevel_resize(
            toplevel,
            (int)JW_BROWSER_WIDTH,
            (int)JW_BROWSER_HEIGHT)) {
        g_printerr("Failed to resize the WPE view to 1280x720\n");
        goto cleanup;
    }
    wpe_view_focus_in(state.wpe_view);

    g_signal_connect(
        state.web_view,
        "load-changed",
        G_CALLBACK(jw_browser_load_changed),
        &state);
    g_signal_connect(
        state.web_view,
        "load-failed",
        G_CALLBACK(jw_browser_load_failed),
        &state);
    g_signal_connect(
        state.web_view,
        "web-process-terminated",
        G_CALLBACK(jw_browser_web_process_terminated),
        &state);
    state.context_source_id = g_timeout_add(
        JW_BROWSER_INPUT_INTERVAL_MS,
        jw_browser_poll_context,
        &state);
    state.signal_int_source_id = g_unix_signal_add(
        SIGINT,
        jw_browser_quit,
        &state);
    state.signal_term_source_id = g_unix_signal_add(
        SIGTERM,
        jw_browser_quit,
        &state);

    webkit_web_view_load_uri(state.web_view, url);
    g_main_loop_run(state.main_loop);
    result = state.runtime_failed ? EXIT_FAILURE : EXIT_SUCCESS;

cleanup:
    jw_browser_clear_secret(vnc_password, vnc_password_length);
    g_clear_pointer(&vnc_password, g_free);
    jw_event_manager_destroy(event_manager);
    jw_proxy_destroy(proxy);
    jw_browser_cleanup(&state);
    return result;
}
