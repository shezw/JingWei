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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wpe/webkit.h>

#define JW_BROWSER_WIDTH 1280U
#define JW_BROWSER_HEIGHT 720U
#define JW_BROWSER_INPUT_CAPACITY 1024U
#define JW_BROWSER_DEFAULT_VNC_PORT 5900
#define JW_BROWSER_INPUT_INTERVAL_MS 8U

#ifndef JW_BROWSER_DEFAULT_URL
#define JW_BROWSER_DEFAULT_URL "about:blank"
#endif

typedef struct jw_browser_state {
    GMainLoop *main_loop;
    jw_surface_t *surface;
    jw_vnc_backend_t *vnc_backend;
    WPEDisplay *display;
    WebKitWebView *web_view;
    WPEView *wpe_view;
    guint input_source_id;
    guint signal_int_source_id;
    guint signal_term_source_id;
    guint pointer_x;
    guint pointer_y;
    guint pointer_buttons;
    jw_wpe_modifier_state_t keyboard_modifiers;
    guint64 input_reset_count;
    int vnc_port;
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

static guint32 jw_browser_event_time(void)
{
    return (guint32)(g_get_monotonic_time() / 1000);
}

static WPEModifiers jw_browser_modifiers(
    const jw_browser_state_t *state,
    guint buttons)
{
    guint modifiers = jw_wpe_modifier_state_get(&state->keyboard_modifiers);

    if ((buttons & 1U) != 0) {
        modifiers |= WPE_MODIFIER_POINTER_BUTTON1;
    }
    if ((buttons & 2U) != 0) {
        modifiers |= WPE_MODIFIER_POINTER_BUTTON2;
    }
    if ((buttons & 4U) != 0) {
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

static void jw_browser_send_pointer_event(
    jw_browser_state_t *state,
    const jw_pointer_event_t *pointer)
{
    static const guint rfb_masks[] = { 1U, 2U, 4U };
    static const guint wpe_buttons[] = {
        WPE_BUTTON_PRIMARY,
        WPE_BUTTON_MIDDLE,
        WPE_BUTTON_SECONDARY
    };
    guint32 time = jw_browser_event_time();
    guint previous_buttons = state->pointer_buttons;
    guint index;

    if (!state->pointer_inside) {
        jw_browser_send_event(
            state,
            wpe_event_pointer_move_new(
                WPE_EVENT_POINTER_ENTER,
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, pointer->buttons),
                pointer->x,
                pointer->y,
                0,
                0));
        state->pointer_inside = TRUE;
    } else if (state->pointer_x != pointer->x ||
        state->pointer_y != pointer->y) {
        jw_browser_send_event(
            state,
            wpe_event_pointer_move_new(
                WPE_EVENT_POINTER_MOVE,
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, pointer->buttons),
                pointer->x,
                pointer->y,
                (double)pointer->x - state->pointer_x,
                (double)pointer->y - state->pointer_y));
    }

    state->pointer_x = pointer->x;
    state->pointer_y = pointer->y;
    state->pointer_buttons = pointer->buttons;

    for (index = 0; index < G_N_ELEMENTS(rfb_masks); ++index) {
        gboolean was_pressed = (previous_buttons & rfb_masks[index]) != 0;
        gboolean is_pressed = (pointer->buttons & rfb_masks[index]) != 0;
        guint press_count;

        if (was_pressed == is_pressed) {
            continue;
        }
        press_count = is_pressed
            ? wpe_view_compute_press_count(
                state->wpe_view,
                pointer->x,
                pointer->y,
                wpe_buttons[index],
                time)
            : 0;
        jw_browser_send_event(
            state,
            wpe_event_pointer_button_new(
                is_pressed ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP,
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, pointer->buttons),
                wpe_buttons[index],
                pointer->x,
                pointer->y,
                press_count));
    }

    if ((pointer->buttons & 8U) != 0 && (previous_buttons & 8U) == 0) {
        jw_browser_send_event(
            state,
            wpe_event_scroll_new(
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, pointer->buttons),
                0,
                -1,
                FALSE,
                FALSE,
                pointer->x,
                pointer->y));
    }
    if ((pointer->buttons & 16U) != 0 && (previous_buttons & 16U) == 0) {
        jw_browser_send_event(
            state,
            wpe_event_scroll_new(
                state->wpe_view,
                WPE_INPUT_SOURCE_MOUSE,
                time,
                jw_browser_modifiers(state, pointer->buttons),
                0,
                1,
                FALSE,
                FALSE,
                pointer->x,
                pointer->y));
    }
}

static void jw_browser_send_key_event(
    jw_browser_state_t *state,
    const jw_key_event_t *key)
{
    jw_wpe_modifier_state_update(
        &state->keyboard_modifiers,
        key->keysym,
        key->pressed);
    jw_browser_send_event(
        state,
        wpe_event_keyboard_new(
            key->pressed
                ? WPE_EVENT_KEYBOARD_KEY_DOWN
                : WPE_EVENT_KEYBOARD_KEY_UP,
            state->wpe_view,
            WPE_INPUT_SOURCE_KEYBOARD,
            jw_browser_event_time(),
            jw_browser_modifiers(state, state->pointer_buttons),
            0,
            key->keysym));
}

static void jw_browser_reset_input(
    jw_browser_state_t *state,
    uint64_t event_id)
{
    wpe_view_focus_out(state->wpe_view);
    state->pointer_buttons = 0;
    jw_wpe_modifier_state_reset(&state->keyboard_modifiers);
    state->input_reset_count += 1;
    wpe_view_focus_in(state->wpe_view);

    g_printerr(
        "JW_WPE_INPUT_RESET count=%" G_GUINT64_FORMAT
        " event_id=%" G_GUINT64_FORMAT "\n",
        state->input_reset_count,
        (guint64)event_id);
    fflush(stderr);
}

static gboolean jw_browser_process_input(gpointer user_data)
{
    jw_browser_state_t *state = user_data;
    jw_input_event_t event;
    jw_status_t status;

    status = jw_vnc_backend_process_events(state->vnc_backend, 0);
    if (status != JW_STATUS_OK) {
        g_printerr(
            "JingWei VNC event processing failed: %s\n",
            jw_status_string(status));
        state->runtime_failed = TRUE;
        g_main_loop_quit(state->main_loop);
        return G_SOURCE_REMOVE;
    }

    while ((status = jw_surface_poll_input(state->surface, &event)) ==
        JW_STATUS_OK) {
        switch (event.type) {
        case JW_INPUT_EVENT_POINTER:
            jw_browser_send_pointer_event(state, &event.data.pointer);
            break;
        case JW_INPUT_EVENT_KEY:
            jw_browser_send_key_event(state, &event.data.key);
            break;
        case JW_EVENT_INPUT_RESET:
            jw_browser_reset_input(state, event.event_id);
            break;
        default:
            break;
        }
    }

    if (status != JW_STATUS_EMPTY) {
        g_printerr(
            "JingWei input queue failed: %s\n",
            jw_status_string(status));
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
    jw_browser_remove_source(&state->input_source_id);
    jw_browser_remove_source(&state->signal_int_source_id);
    jw_browser_remove_source(&state->signal_term_source_id);
    g_clear_object(&state->web_view);
    state->wpe_view = NULL;
    g_clear_object(&state->display);
    if (state->vnc_backend != NULL) {
        (void)jw_vnc_backend_stop(state->vnc_backend);
        jw_vnc_backend_destroy(state->vnc_backend);
    }
    if (state->surface != NULL) {
        jw_surface_destroy(state->surface);
    }
    if (state->main_loop != NULL) {
        g_main_loop_unref(state->main_loop);
    }
}

int main(int argc, char **argv)
{
    const char *url = argc > 1 ? argv[1] : JW_BROWSER_DEFAULT_URL;
    const char *port_value = g_getenv("VNC_PORT");
    const char *cache_model_value = g_getenv("JINGWEI_CACHE_MODEL");
    jw_browser_state_t state = { 0 };
    jw_vnc_config_t vnc_config;
    jw_status_t status;
    WebKitCacheModel cache_model;
    WPEToplevel *toplevel;
    GError *error = NULL;
    int port;
    int result = EXIT_FAILURE;

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

    state.vnc_port = port;
    state.main_loop = g_main_loop_new(NULL, FALSE);
    state.surface = jw_surface_create(
        JW_BROWSER_WIDTH,
        JW_BROWSER_HEIGHT,
        JW_BROWSER_INPUT_CAPACITY);
    if (state.surface == NULL) {
        g_printerr("Failed to create the JingWei browser surface\n");
        goto cleanup;
    }

    vnc_config.port = port;
    vnc_config.desktop_name = "JingWei WPE Browser";
    state.vnc_backend = jw_vnc_backend_create(state.surface, &vnc_config);
    if (state.vnc_backend == NULL) {
        g_printerr("Failed to create the JingWei VNC backend\n");
        goto cleanup;
    }
    status = jw_vnc_backend_start(state.vnc_backend);
    if (status != JW_STATUS_OK) {
        g_printerr(
            "Failed to start the JingWei VNC backend: %s\n",
            jw_status_string(status));
        goto cleanup;
    }

    state.display = jw_wpe_display_new(state.surface, state.vnc_backend);
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
    state.input_source_id = g_timeout_add(
        JW_BROWSER_INPUT_INTERVAL_MS,
        jw_browser_process_input,
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
    jw_browser_cleanup(&state);
    return result;
}
