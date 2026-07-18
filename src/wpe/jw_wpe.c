/*
    JingWei
    src/wpe jw_wpe.c    2026-07-18

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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string.h>
#include <wpe/headless/wpe-headless.h>

#define JW_WPE_FRAME_INTERVAL_US (G_USEC_PER_SEC / 60)

typedef struct _JWWPEView JWWPEView;
typedef struct _JWWPEViewClass JWWPEViewClass;
typedef struct _JWWPEDisplay JWWPEDisplay;
typedef struct _JWWPEDisplayClass JWWPEDisplayClass;

struct _JWWPEView {
    WPEView parent_instance;
    jw_display_t *platform_display;
    WPEBuffer *pending_buffer;
    WPEBuffer *committed_buffer;
    GSource *frame_source;
    gint64 last_frame_time;
};

struct _JWWPEViewClass {
    WPEViewClass parent_class;
};

struct _JWWPEDisplay {
    WPEDisplay parent_instance;
    jw_display_t *platform_display;
    gboolean connected;
    guint64 frame_count;
};

struct _JWWPEDisplayClass {
    WPEDisplayClass parent_class;
};

G_DEFINE_TYPE(JWWPEView, jw_wpe_view, WPE_TYPE_VIEW)
G_DEFINE_TYPE(JWWPEDisplay, jw_wpe_display, WPE_TYPE_DISPLAY)

enum {
    JW_WPE_DISPLAY_FIRST_FRAME,
    JW_WPE_DISPLAY_LAST_SIGNAL
};

static guint jw_wpe_display_signals[JW_WPE_DISPLAY_LAST_SIGNAL];

static gboolean jw_wpe_frame_source_dispatch(
    GSource *source,
    GSourceFunc callback,
    gpointer user_data)
{
    if (g_source_get_ready_time(source) == -1) {
        return G_SOURCE_CONTINUE;
    }
    g_source_set_ready_time(source, -1);
    return callback(user_data);
}

static GSourceFuncs jw_wpe_frame_source_functions = {
    NULL,
    NULL,
    jw_wpe_frame_source_dispatch,
    NULL,
    NULL,
    NULL
};

static gboolean jw_wpe_view_commit_frame(gpointer user_data)
{
    WPEView *view = WPE_VIEW(user_data);
    JWWPEView *jw_view = (JWWPEView *)view;

    if (jw_view->committed_buffer != NULL) {
        wpe_view_buffer_released(view, jw_view->committed_buffer);
        g_clear_object(&jw_view->committed_buffer);
    }
    jw_view->committed_buffer = g_steal_pointer(&jw_view->pending_buffer);
    if (jw_view->committed_buffer != NULL) {
        wpe_view_buffer_rendered(view, jw_view->committed_buffer);
    }
    if (jw_view->frame_source == NULL ||
        g_source_is_destroyed(jw_view->frame_source)) {
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean jw_wpe_has_extension(
    const char *extensions,
    const char *extension)
{
    const char *match;
    size_t length;

    if (extensions == NULL || extension == NULL || extension[0] == '\0') {
        return FALSE;
    }

    length = strlen(extension);
    match = extensions;
    while ((match = strstr(match, extension)) != NULL) {
        if ((match == extensions || match[-1] == ' ') &&
            (match[length] == '\0' || match[length] == ' ')) {
            return TRUE;
        }
        match += length;
    }
    return FALSE;
}

static void jw_wpe_view_toplevel_changed(
    WPEView *view,
    GParamSpec *param_spec,
    gpointer user_data)
{
    WPEToplevel *toplevel;
    int width;
    int height;

    (void)param_spec;
    (void)user_data;

    toplevel = wpe_view_get_toplevel(view);
    if (toplevel == NULL) {
        wpe_view_unmap(view);
        return;
    }

    wpe_toplevel_get_size(toplevel, &width, &height);
    if (width > 0 && height > 0) {
        wpe_view_resized(view, width, height);
    }
    wpe_view_map(view);
}

static void jw_wpe_view_constructed(GObject *object)
{
    G_OBJECT_CLASS(jw_wpe_view_parent_class)->constructed(object);

    g_signal_connect(
        object,
        "notify::toplevel",
        G_CALLBACK(jw_wpe_view_toplevel_changed),
        NULL);

    {
        JWWPEView *view = (JWWPEView *)object;

        view->frame_source = g_source_new(
            &jw_wpe_frame_source_functions,
            sizeof(GSource));
        g_source_set_name(view->frame_source, "JingWei WPE frame timer");
        g_source_set_callback(
            view->frame_source,
            jw_wpe_view_commit_frame,
            object,
            NULL);
        g_source_attach(
            view->frame_source,
            g_main_context_get_thread_default());
        g_source_set_ready_time(view->frame_source, -1);
    }
}

static void jw_wpe_view_dispose(GObject *object)
{
    JWWPEView *view = (JWWPEView *)object;

    if (view->frame_source != NULL) {
        g_source_destroy(view->frame_source);
        g_clear_pointer(&view->frame_source, g_source_unref);
    }
    g_clear_object(&view->pending_buffer);
    g_clear_object(&view->committed_buffer);

    G_OBJECT_CLASS(jw_wpe_view_parent_class)->dispose(object);
}

static gboolean jw_wpe_frame_size_is_valid(
    int width,
    int height,
    guint stride,
    gsize size)
{
    gsize row_bytes;
    gsize required_size;

    if (width <= 0 || height <= 0 || stride > G_MAXINT ||
        (gsize)width > G_MAXSIZE / 4U) {
        return FALSE;
    }

    row_bytes = (gsize)width * 4U;
    if ((gsize)stride < row_bytes ||
        (gsize)(height - 1) > (G_MAXSIZE - row_bytes) / stride) {
        return FALSE;
    }

    required_size = (gsize)(height - 1) * stride + row_bytes;
    return size >= required_size;
}

static gboolean jw_wpe_rects_from_rectangles(
    const WPERectangle *rectangles,
    guint rectangle_count,
    int frame_width,
    int frame_height,
    jw_rect_t **rects,
    GError **error)
{
    guint index;

    *rects = NULL;
    if (rectangle_count == 0) {
        return TRUE;
    }
    if (rectangle_count > G_MAXINT) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "WPE supplied too many damage rectangles");
        return FALSE;
    }
    if (rectangles == NULL) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "WPE supplied a non-zero damage count without rectangles");
        return FALSE;
    }

    *rects = g_try_new(jw_rect_t, rectangle_count);
    if (*rects == NULL) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Failed to allocate JingWei damage rectangles");
        return FALSE;
    }

    for (index = 0; index < rectangle_count; ++index) {
        const WPERectangle *rectangle = &rectangles[index];

        if (rectangle->x < 0 || rectangle->y < 0 ||
            rectangle->width <= 0 || rectangle->height <= 0 ||
            rectangle->x >= frame_width || rectangle->y >= frame_height ||
            rectangle->width > frame_width - rectangle->x ||
            rectangle->height > frame_height - rectangle->y) {
            g_set_error(
                error,
                WPE_VIEW_ERROR,
                WPE_VIEW_ERROR_RENDER_FAILED,
                "Invalid WPE damage rectangle %u: %d,%d %dx%d",
                index,
                rectangle->x,
                rectangle->y,
                rectangle->width,
                rectangle->height);
            g_clear_pointer(rects, g_free);
            return FALSE;
        }

        (*rects)[index].x = rectangle->x;
        (*rects)[index].y = rectangle->y;
        (*rects)[index].w = rectangle->width;
        (*rects)[index].h = rectangle->height;
    }
    return TRUE;
}

static gboolean jw_wpe_view_render_buffer(
    WPEView *view,
    WPEBuffer *buffer,
    const WPERectangle *damage_rectangles,
    guint damage_count,
    GError **error)
{
    JWWPEView *jw_view = (JWWPEView *)view;
    WPEBufferSHM *shm_buffer;
    GBytes *bytes;
    const void *pixels;
    gsize size;
    int width;
    int height;
    guint stride;
    jw_buffer_t *jingwei_buffer;
    jw_rect_t *rects = NULL;
    int present_result;
    gint64 now;
    gint64 next_frame_time;

    if (jw_view->frame_source == NULL ||
        g_source_is_destroyed(jw_view->frame_source)) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "JingWei WPE frame source is unavailable");
        return FALSE;
    }

    if (!WPE_IS_BUFFER_SHM(buffer)) {
        g_set_error(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "JingWei only accepts WPEBufferSHM, received %s",
            G_OBJECT_TYPE_NAME(buffer));
        return FALSE;
    }

    shm_buffer = WPE_BUFFER_SHM(buffer);
    if (wpe_buffer_shm_get_format(shm_buffer) != WPE_PIXEL_FORMAT_ARGB8888) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "JingWei only accepts WPE SHM ARGB8888 buffers");
        return FALSE;
    }

#if G_BYTE_ORDER != G_LITTLE_ENDIAN
    g_set_error_literal(
        error,
        WPE_VIEW_ERROR,
        WPE_VIEW_ERROR_RENDER_FAILED,
        "WPE SHM ARGB8888 requires a little-endian host");
    return FALSE;
#endif

    width = wpe_buffer_get_width(buffer);
    height = wpe_buffer_get_height(buffer);
    stride = wpe_buffer_shm_get_stride(shm_buffer);
    bytes = wpe_buffer_shm_get_data(shm_buffer);
    pixels = bytes != NULL ? g_bytes_get_data(bytes, &size) : NULL;
    if (pixels == NULL ||
        !jw_wpe_frame_size_is_valid(width, height, stride, size)) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "WPE SHM buffer dimensions, stride, or data size are invalid");
        return FALSE;
    }

    if (!jw_wpe_rects_from_rectangles(
            damage_rectangles,
            damage_count,
            width,
            height,
            &rects,
            error)) {
        return FALSE;
    }

    jingwei_buffer = jw_buffer_wrap_pixels(
        width,
        height,
        (int)stride,
        JW_PIXEL_FORMAT_ARGB8888,
        (void *)pixels,
        NULL,
        NULL);
    if (jingwei_buffer == NULL) {
        g_free(rects);
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Failed to wrap the WPE SHM buffer for JingWei");
        return FALSE;
    }
    present_result = jw_display_present_buffer_rects(
        jw_view->platform_display,
        jingwei_buffer,
        rects,
        (int)damage_count);
    jw_buffer_destroy(jingwei_buffer);
    g_free(rects);

    if (present_result != 0) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "JingWei rejected the WPE frame");
        return FALSE;
    }

    {
        JWWPEDisplay *display =
            (JWWPEDisplay *)wpe_view_get_display(view);

        display->frame_count += 1;
        if (display->frame_count == 1) {
            g_signal_emit(
                display,
                jw_wpe_display_signals[JW_WPE_DISPLAY_FIRST_FRAME],
                0);
        }
    }

    g_set_object(&jw_view->pending_buffer, buffer);
    now = g_get_monotonic_time();
    if (jw_view->last_frame_time == 0) {
        jw_view->last_frame_time = now;
    }
    next_frame_time = jw_view->last_frame_time + JW_WPE_FRAME_INTERVAL_US;
    jw_view->last_frame_time = now;
    g_source_set_ready_time(
        jw_view->frame_source,
        next_frame_time <= now ? 0 : next_frame_time);
    return TRUE;
}

static void jw_wpe_view_class_init(JWWPEViewClass *view_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(view_class);
    WPEViewClass *wpe_view_class = WPE_VIEW_CLASS(view_class);

    object_class->constructed = jw_wpe_view_constructed;
    object_class->dispose = jw_wpe_view_dispose;
    wpe_view_class->render_buffer = jw_wpe_view_render_buffer;
}

static void jw_wpe_view_init(JWWPEView *view)
{
    view->platform_display = NULL;
    view->pending_buffer = NULL;
    view->committed_buffer = NULL;
    view->frame_source = NULL;
    view->last_frame_time = 0;
}

static gboolean jw_wpe_display_connect(
    WPEDisplay *display,
    GError **error)
{
    JWWPEDisplay *jw_display = (JWWPEDisplay *)display;

    (void)error;
    jw_display->connected = TRUE;
    return TRUE;
}

static WPEView *jw_wpe_display_create_view(WPEDisplay *display)
{
    JWWPEDisplay *jw_display = (JWWPEDisplay *)display;
    JWWPEView *view;

    view = g_object_new(jw_wpe_view_get_type(), "display", display, NULL);
    view->platform_display = jw_display->platform_display;
    return WPE_VIEW(view);
}

static gpointer jw_wpe_display_get_egl_display(
    WPEDisplay *display,
    GError **error)
{
    const char *extensions;
    EGLDisplay egl_display;

    (void)display;
    extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!jw_wpe_has_extension(extensions, "EGL_MESA_platform_surfaceless")) {
        g_set_error_literal(
            error,
            WPE_EGL_ERROR,
            WPE_EGL_ERROR_NOT_AVAILABLE,
            "Surfaceless EGL is not available");
        return NULL;
    }

#if defined(EGL_VERSION_1_5)
    egl_display = eglGetPlatformDisplay(
        EGL_PLATFORM_SURFACELESS_MESA,
        EGL_DEFAULT_DISPLAY,
        NULL);
#else
    {
        PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display;

        get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
            eglGetProcAddress("eglGetPlatformDisplayEXT");
        egl_display = get_platform_display != NULL
            ? get_platform_display(
                EGL_PLATFORM_SURFACELESS_MESA,
                EGL_DEFAULT_DISPLAY,
                NULL)
            : EGL_NO_DISPLAY;
    }
#endif
    if (egl_display != EGL_NO_DISPLAY) {
        return egl_display;
    }

    g_set_error_literal(
        error,
        WPE_EGL_ERROR,
        WPE_EGL_ERROR_NOT_AVAILABLE,
        "Failed to create a surfaceless EGL display");
    return NULL;
}

static WPEToplevel *jw_wpe_display_create_toplevel(
    WPEDisplay *display,
    guint max_views)
{
    return WPE_TOPLEVEL(g_object_new(
        WPE_TYPE_TOPLEVEL_HEADLESS,
        "display",
        display,
        "max-views",
        max_views,
        NULL));
}

static void jw_wpe_display_class_init(JWWPEDisplayClass *display_class)
{
    WPEDisplayClass *wpe_display_class = WPE_DISPLAY_CLASS(display_class);

    wpe_display_class->connect = jw_wpe_display_connect;
    wpe_display_class->create_view = jw_wpe_display_create_view;
    wpe_display_class->get_egl_display = jw_wpe_display_get_egl_display;
    wpe_display_class->create_toplevel = jw_wpe_display_create_toplevel;

    jw_wpe_display_signals[JW_WPE_DISPLAY_FIRST_FRAME] = g_signal_new(
        "first-frame",
        G_TYPE_FROM_CLASS(display_class),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0);
}

static void jw_wpe_display_init(JWWPEDisplay *display)
{
    display->platform_display = NULL;
    display->connected = FALSE;
    display->frame_count = 0;
    wpe_display_set_available_input_devices(
        WPE_DISPLAY(display),
        WPE_AVAILABLE_INPUT_DEVICE_MOUSE |
            WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
}

WPEDisplay *jw_wpe_display_new(jw_display_t *platform_display)
{
    JWWPEDisplay *display;

    g_return_val_if_fail(platform_display != NULL, NULL);

    display = g_object_new(jw_wpe_display_get_type(), NULL);
    display->platform_display = platform_display;
    return WPE_DISPLAY(display);
}

guint64 jw_wpe_display_get_frame_count(WPEDisplay *display)
{
    JWWPEDisplay *jw_display;

    g_return_val_if_fail(
        G_TYPE_CHECK_INSTANCE_TYPE(display, JW_TYPE_WPE_DISPLAY),
        0);

    jw_display = (JWWPEDisplay *)display;
    return jw_display->frame_count;
}
