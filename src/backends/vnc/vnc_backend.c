/**
    -----------------------------------------------------------

    Project JingWei
    backends vnc_backend.c    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#include <jingwei/vnc.h>

#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#if JINGWEI_HAVE_LIBVNCSERVER
#include <rfb/rfb.h>
#endif

struct jw_vnc_backend {
    jw_surface_t *surface;
    uint8_t *frame_buffer;
    size_t frame_buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int port;
    char *desktop_name;
    int started;
    pthread_mutex_t mutex;
    int mutex_initialized;
#if JINGWEI_HAVE_LIBVNCSERVER
    rfbScreenInfoPtr screen;
#endif
};

#if JINGWEI_HAVE_LIBVNCSERVER
static char *jw_vnc_copy_string(const char *source)
{
    size_t length;
    char *copy;

    if (source == NULL) {
        return NULL;
    }

    length = strlen(source);
    copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, source, length + 1);
    }
    return copy;
}
#endif

jw_status_t jw_vnc_enqueue_pointer(
    jw_surface_t *surface,
    int button_mask,
    int x,
    int y,
    uint64_t *event_id)
{
    jw_surface_info_t info;
    jw_input_event_t event;
    jw_status_t status;

    if (surface == NULL || button_mask < 0 || x < 0 || y < 0) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    status = jw_surface_get_info(surface, &info);
    if (status != JW_STATUS_OK) {
        return status;
    }
    if ((uint32_t)x >= info.width || (uint32_t)y >= info.height) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_POINTER;
    event.data.pointer.x = (uint32_t)x;
    event.data.pointer.y = (uint32_t)y;
    event.data.pointer.buttons = (uint32_t)button_mask;
    return jw_surface_enqueue_input(surface, &event, event_id);
}

jw_status_t jw_vnc_enqueue_key(
    jw_surface_t *surface,
    int pressed,
    uint32_t keysym,
    uint64_t *event_id)
{
    jw_input_event_t event;

    if (surface == NULL || (pressed != 0 && pressed != 1)) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_KEY;
    event.data.key.keysym = keysym;
    event.data.key.pressed = pressed;
    return jw_surface_enqueue_input(surface, &event, event_id);
}

int jw_vnc_backend_is_available(void)
{
#if JINGWEI_HAVE_LIBVNCSERVER
    return 1;
#else
    return 0;
#endif
}

#if JINGWEI_HAVE_LIBVNCSERVER
static void jw_vnc_pointer_callback(
    int button_mask,
    int x,
    int y,
    rfbClientPtr client)
{
    jw_vnc_backend_t *backend;

    if (client == NULL || client->screen == NULL) {
        return;
    }
    backend = (jw_vnc_backend_t *)client->screen->screenData;
    if (backend != NULL) {
        (void)jw_vnc_enqueue_pointer(
            backend->surface, button_mask, x, y, NULL);
    }
}

static void jw_vnc_key_callback(
    rfbBool pressed,
    rfbKeySym keysym,
    rfbClientPtr client)
{
    jw_vnc_backend_t *backend;

    if (client == NULL || client->screen == NULL) {
        return;
    }
    backend = (jw_vnc_backend_t *)client->screen->screenData;
    if (backend != NULL) {
        (void)jw_vnc_enqueue_key(
            backend->surface, pressed ? 1 : 0, (uint32_t)keysym, NULL);
    }
}
#endif

jw_vnc_backend_t *jw_vnc_backend_create(
    jw_surface_t *surface,
    const jw_vnc_config_t *config)
{
#if JINGWEI_HAVE_LIBVNCSERVER
    jw_surface_info_t info;
    jw_vnc_backend_t *backend;
    const char *desktop_name;
    int argc = 0;
    char **argv = NULL;

    if (surface == NULL || jw_surface_get_info(surface, &info) != JW_STATUS_OK ||
        info.width > (uint32_t)(INT_MAX / 4) ||
        info.height > (uint32_t)INT_MAX || info.size > (size_t)INT_MAX) {
        return NULL;
    }
    if (config != NULL && (config->port < 0 || config->port > 65535)) {
        return NULL;
    }

    backend = (jw_vnc_backend_t *)calloc(1, sizeof(*backend));
    if (backend == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&backend->mutex, NULL) != 0) {
        free(backend);
        return NULL;
    }
    backend->mutex_initialized = 1;

    desktop_name = config != NULL && config->desktop_name != NULL
        ? config->desktop_name : "JingWei";
    backend->desktop_name = jw_vnc_copy_string(desktop_name);
    backend->frame_buffer = (uint8_t *)calloc(1, info.size);
    if (backend->desktop_name == NULL || backend->frame_buffer == NULL) {
        jw_vnc_backend_destroy(backend);
        return NULL;
    }

    backend->surface = surface;
    backend->frame_buffer_size = info.size;
    backend->width = info.width;
    backend->height = info.height;
    backend->stride = info.stride;
    backend->port = config != NULL && config->port != 0 ? config->port : 5900;
    backend->screen = rfbGetScreen(
        &argc, argv, (int)info.width, (int)info.height, 8, 3, 4);
    if (backend->screen == NULL) {
        jw_vnc_backend_destroy(backend);
        return NULL;
    }

    backend->screen->frameBuffer = (char *)backend->frame_buffer;
    backend->screen->desktopName = backend->desktop_name;
    backend->screen->screenData = backend;
    backend->screen->port = backend->port;
    backend->screen->ipv6port = 0;
    backend->screen->alwaysShared = TRUE;
    backend->screen->ptrAddEvent = jw_vnc_pointer_callback;
    backend->screen->kbdAddEvent = jw_vnc_key_callback;
    backend->screen->serverFormat.bitsPerPixel = 32;
    backend->screen->serverFormat.depth = 24;
    backend->screen->serverFormat.trueColour = TRUE;
    backend->screen->serverFormat.redMax = 255;
    backend->screen->serverFormat.greenMax = 255;
    backend->screen->serverFormat.blueMax = 255;
    backend->screen->serverFormat.redShift = 16;
    backend->screen->serverFormat.greenShift = 8;
    backend->screen->serverFormat.blueShift = 0;
    return backend;
#else
    (void)surface;
    (void)config;
    return NULL;
#endif
}

void jw_vnc_backend_destroy(jw_vnc_backend_t *backend)
{
    if (backend == NULL) {
        return;
    }

#if JINGWEI_HAVE_LIBVNCSERVER
    if (backend->started) {
        rfbShutdownServer(backend->screen, TRUE);
    }
    if (backend->screen != NULL) {
        rfbScreenCleanup(backend->screen);
    }
#endif
    if (backend->mutex_initialized) {
        pthread_mutex_destroy(&backend->mutex);
    }
    free(backend->desktop_name);
    free(backend->frame_buffer);
    free(backend);
}

jw_status_t jw_vnc_backend_start(jw_vnc_backend_t *backend)
{
#if JINGWEI_HAVE_LIBVNCSERVER
    if (backend == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    pthread_mutex_lock(&backend->mutex);
    if (backend->started) {
        pthread_mutex_unlock(&backend->mutex);
        return JW_STATUS_INVALID_STATE;
    }

    rfbInitServer(backend->screen);
    if (backend->screen->listenSock == RFB_INVALID_SOCKET ||
        backend->screen->listen6Sock != RFB_INVALID_SOCKET) {
        rfbShutdownServer(backend->screen, TRUE);
        pthread_mutex_unlock(&backend->mutex);
        return JW_STATUS_IO_ERROR;
    }
    backend->started = 1;
    pthread_mutex_unlock(&backend->mutex);
    return JW_STATUS_OK;
#else
    (void)backend;
    return JW_STATUS_UNAVAILABLE;
#endif
}

jw_status_t jw_vnc_backend_process_events(
    jw_vnc_backend_t *backend,
    long timeout_microseconds)
{
#if JINGWEI_HAVE_LIBVNCSERVER
    if (backend == NULL || timeout_microseconds < 0) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    pthread_mutex_lock(&backend->mutex);
    if (!backend->started) {
        pthread_mutex_unlock(&backend->mutex);
        return JW_STATUS_INVALID_STATE;
    }

    rfbProcessEvents(backend->screen, timeout_microseconds);
    pthread_mutex_unlock(&backend->mutex);
    return JW_STATUS_OK;
#else
    (void)backend;
    (void)timeout_microseconds;
    return JW_STATUS_UNAVAILABLE;
#endif
}

jw_status_t jw_vnc_backend_stop(jw_vnc_backend_t *backend)
{
#if JINGWEI_HAVE_LIBVNCSERVER
    if (backend == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }
    pthread_mutex_lock(&backend->mutex);
    if (!backend->started) {
        pthread_mutex_unlock(&backend->mutex);
        return JW_STATUS_INVALID_STATE;
    }

    rfbShutdownServer(backend->screen, TRUE);
    backend->started = 0;
    pthread_mutex_unlock(&backend->mutex);
    return JW_STATUS_OK;
#else
    (void)backend;
    return JW_STATUS_UNAVAILABLE;
#endif
}

jw_status_t jw_vnc_backend_publish_frame(
    jw_vnc_backend_t *backend,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count,
    uint64_t *serial)
{
#if JINGWEI_HAVE_LIBVNCSERVER
    jw_status_t status;
    uint64_t submitted_serial;
    size_t index;

    if (backend == NULL) {
        return JW_STATUS_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&backend->mutex);
    status = jw_surface_submit_frame_and_copy(
        backend->surface,
        frame,
        damages,
        damage_count,
        backend->frame_buffer,
        backend->frame_buffer_size,
        backend->stride,
        &submitted_serial);
    if (status != JW_STATUS_OK) {
        pthread_mutex_unlock(&backend->mutex);
        return status;
    }

    if (submitted_serial == 1 || damage_count == 0) {
        rfbMarkRectAsModified(
            backend->screen, 0, 0, (int)backend->width, (int)backend->height);
    } else {
        for (index = 0; index < damage_count; ++index) {
            const jw_damage_t *damage = &damages[index];
            rfbMarkRectAsModified(
                backend->screen,
                (int)damage->x,
                (int)damage->y,
                (int)(damage->x + damage->width),
                (int)(damage->y + damage->height));
        }
    }

    if (serial != NULL) {
        *serial = submitted_serial;
    }
    pthread_mutex_unlock(&backend->mutex);
    return JW_STATUS_OK;
#else
    (void)backend;
    (void)frame;
    (void)damages;
    (void)damage_count;
    (void)serial;
    return JW_STATUS_UNAVAILABLE;
#endif
}
