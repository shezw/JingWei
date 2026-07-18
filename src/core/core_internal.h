/**
    -----------------------------------------------------------

    Project JingWei
    core core_internal.h    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#ifndef JINGWEI_CORE_INTERNAL_H
#define JINGWEI_CORE_INTERNAL_H

#include <pthread.h>

#include <jingwei/core.h>

struct jw_event_queue {
    jw_input_event_t *events;
    size_t capacity;
    size_t head;
    size_t count;
    uint64_t overflow_count;
    uint64_t next_event_id;
    uint32_t last_pointer_buttons;
    int has_pointer_state;
    pthread_mutex_t mutex;
};

struct jw_surface {
    uint8_t *front_buffer;
    size_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint64_t serial;
    pthread_mutex_t frame_mutex;
    jw_event_queue_t *input_queue;
};

#endif
