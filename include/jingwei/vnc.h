/**
    -----------------------------------------------------------

    Project JingWei
    backends vnc.h    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#ifndef JINGWEI_VNC_H
#define JINGWEI_VNC_H

#include <stddef.h>
#include <stdint.h>

#include <jingwei/core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jw_vnc_backend jw_vnc_backend_t;

typedef struct jw_vnc_config {
    /* IPv4 TCP port. Zero selects the default RFB port 5900. */
    int port;
    const char *desktop_name;
} jw_vnc_config_t;

int jw_vnc_backend_is_available(void);

/*
 * The backend borrows surface. The caller must keep surface alive and destroy
 * the backend before calling jw_surface_destroy(). Before destroy, stop new
 * calls and join every thread that may be inside process_events, stop, or
 * publish_frame; destroy does not synchronize with concurrent callers. The
 * backend is IPv4-only; IPv6 listening is deliberately disabled.
 */
jw_vnc_backend_t *jw_vnc_backend_create(
    jw_surface_t *surface,
    const jw_vnc_config_t *config);
void jw_vnc_backend_destroy(jw_vnc_backend_t *backend);
jw_status_t jw_vnc_backend_start(jw_vnc_backend_t *backend);
jw_status_t jw_vnc_backend_process_events(
    jw_vnc_backend_t *backend,
    long timeout_microseconds);
jw_status_t jw_vnc_backend_stop(jw_vnc_backend_t *backend);

jw_status_t jw_vnc_backend_publish_frame(
    jw_vnc_backend_t *backend,
    const jw_frame_t *frame,
    const jw_damage_t *damages,
    size_t damage_count,
    uint64_t *serial);

/* These helpers are the same mapping path used by LibVNCServer callbacks. */
jw_status_t jw_vnc_enqueue_pointer(
    jw_surface_t *surface,
    int button_mask,
    int x,
    int y,
    uint64_t *event_id);
jw_status_t jw_vnc_enqueue_key(
    jw_surface_t *surface,
    int pressed,
    uint32_t keysym,
    uint64_t *event_id);

#ifdef __cplusplus
}
#endif

#endif
