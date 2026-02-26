#ifndef JW_MT_PROTOCOL_H
#define JW_MT_PROTOCOL_H

#include <stdint.h>

#define JW_MT_SOCKET_PATH "/tmp/jw_mt_core.sock"

enum jw_mt_request_type {
    JW_MT_REQ_CREATE_DISPLAY,
    JW_MT_REQ_CREATE_CANVAS,
    JW_MT_REQ_COMMIT_DRAWING
};

enum jw_mt_display_type {
    JW_MT_DISPLAY_SDL = 0
};

struct jw_mt_request {
    enum jw_mt_request_type type;
    union {
        struct {
            enum jw_mt_display_type type;
            char name[64];
            int width;
            int height;
        } create_display;

        struct {
            int display_id;
            int width;
            int height;
        } create_canvas;

        struct {
            int display_id;
            int canvas_id; // Usually same as shm_fd in this simple example logic, or mapped ID
            int x;
            int y;
            int w;
            int h;
        } commit_drawing;
    } params;
};

struct jw_mt_response {
    int status; // 0 success, <0 error
    union {
        int display_id;
        struct {
            int canvas_id;
            int size;
        } canvas_info;
    } result;
};

#endif // JW_MT_PROTOCOL_H
