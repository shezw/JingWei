/**
    -----------------------------------------------------------

    Project JingWei
    tests test_core.c    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#include <jingwei/core.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "check failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while (0)

static int test_frame_stride_and_damage(void)
{
    jw_surface_t *surface;
    jw_frame_t frame;
    jw_damage_t first_damage;
    jw_damage_t damage;
    uint8_t source[24];
    uint8_t output[16];
    uint8_t expected[16];
    uint64_t serial;
    size_t index;

    surface = jw_surface_create(2, 2, 4);
    TEST_CHECK(surface != NULL);

    memset(source, 0xee, sizeof(source));
    for (index = 0; index < 8; ++index) {
        source[index] = (uint8_t)(index + 1);
        source[12 + index] = (uint8_t)(index + 9);
    }
    frame.pixels = source;
    frame.size = sizeof(source);
    frame.width = 2;
    frame.height = 2;
    frame.stride = 12;
    frame.format = JW_PIXEL_FORMAT_ARGB8888;

    first_damage.x = 0;
    first_damage.y = 0;
    first_damage.width = 1;
    first_damage.height = 1;
    TEST_CHECK(jw_surface_submit_frame(
        surface, &frame, &first_damage, 1, &serial) ==
        JW_STATUS_OK);
    TEST_CHECK(serial == 1);
    TEST_CHECK(jw_surface_copy_front_buffer(
        surface, output, sizeof(output), 8, &serial) == JW_STATUS_OK);
    TEST_CHECK(serial == 1);
    for (index = 0; index < 8; ++index) {
        expected[index] = (uint8_t)(index + 1);
        expected[8 + index] = (uint8_t)(index + 9);
    }
    TEST_CHECK(memcmp(output, expected, sizeof(output)) == 0);

    memset(source, 0x7a, sizeof(source));
    damage.x = 1;
    damage.y = 0;
    damage.width = 1;
    damage.height = 2;
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, &damage, 1, &serial) ==
        JW_STATUS_OK);
    TEST_CHECK(serial == 2);
    TEST_CHECK(jw_surface_copy_front_buffer(
        surface, output, sizeof(output), 8, &serial) == JW_STATUS_OK);
    TEST_CHECK(serial == 2);

    TEST_CHECK(memcmp(output, expected, 4) == 0);
    TEST_CHECK(output[4] == 0x7a && output[5] == 0x7a &&
        output[6] == 0x7a && output[7] == 0x7a);
    TEST_CHECK(memcmp(output + 8, expected + 8, 4) == 0);
    TEST_CHECK(output[12] == 0x7a && output[13] == 0x7a &&
        output[14] == 0x7a && output[15] == 0x7a);

    jw_surface_destroy(surface);
    return 1;
}

static int test_invalid_frames(void)
{
    jw_surface_t *surface;
    jw_frame_t frame;
    jw_damage_t damage;
    uint8_t pixels[16] = { 0 };
    uint64_t serial;

    surface = jw_surface_create(2, 2, 2);
    TEST_CHECK(surface != NULL);
    TEST_CHECK(jw_surface_create((uint32_t)(INT_MAX / 4) + 1U, 1, 1) == NULL);
    TEST_CHECK(jw_surface_create(1, (uint32_t)(INT_MAX / 4) + 1U, 1) == NULL);

    frame.pixels = pixels;
    frame.size = sizeof(pixels);
    frame.width = 2;
    frame.height = 2;
    frame.stride = 8;
    frame.format = JW_PIXEL_FORMAT_ARGB8888;

    TEST_CHECK(jw_surface_submit_frame(surface, &frame, NULL, 0, &serial) ==
        JW_STATUS_OK);
    TEST_CHECK(serial == 1);

    frame.stride = 7;
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, NULL, 0, &serial) ==
        JW_STATUS_INVALID_ARGUMENT);
    frame.stride = 8;
    frame.size = 15;
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, NULL, 0, &serial) ==
        JW_STATUS_INVALID_ARGUMENT);
    frame.size = sizeof(pixels);
    frame.width = 1;
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, NULL, 0, &serial) ==
        JW_STATUS_INVALID_ARGUMENT);
    frame.width = 2;
    frame.format = (jw_pixel_format_t)99;
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, NULL, 0, &serial) ==
        JW_STATUS_UNSUPPORTED);
    frame.format = JW_PIXEL_FORMAT_ARGB8888;
    damage.x = 1;
    damage.y = 0;
    damage.width = 2;
    damage.height = 1;
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, &damage, 1, &serial) ==
        JW_STATUS_INVALID_ARGUMENT);
    TEST_CHECK(jw_surface_submit_frame(surface, &frame, NULL, 1, &serial) ==
        JW_STATUS_INVALID_ARGUMENT);

    TEST_CHECK(jw_surface_get_info(surface, &(jw_surface_info_t){ 0 }) ==
        JW_STATUS_OK);
    {
        jw_surface_info_t info;
        TEST_CHECK(jw_surface_get_info(surface, &info) == JW_STATUS_OK);
        TEST_CHECK(info.serial == 1);
    }

    jw_surface_destroy(surface);
    return 1;
}

static int test_event_fifo(void)
{
    jw_event_queue_t *queue;
    jw_input_event_t event;
    jw_input_event_t output;
    jw_event_queue_stats_t stats;
    uint64_t first_id;
    uint64_t second_id;

    queue = jw_event_queue_create(2);
    TEST_CHECK(queue != NULL);

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_KEY;
    event.data.key.keysym = 10;
    event.data.key.pressed = 1;
    TEST_CHECK(jw_event_queue_push(queue, &event, &first_id) == JW_STATUS_OK);
    event.data.key.keysym = 20;
    TEST_CHECK(jw_event_queue_push(queue, &event, &second_id) == JW_STATUS_OK);
    TEST_CHECK(first_id == 1 && second_id == 2);
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_FULL);

    TEST_CHECK(jw_event_queue_get_stats(queue, &stats) == JW_STATUS_OK);
    TEST_CHECK(stats.capacity == 2 && stats.pending == 2);
    TEST_CHECK(stats.overflow_count == 1 && stats.next_event_id == 3);

    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 1 && output.data.key.keysym == 10);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 2 && output.data.key.keysym == 20);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_EMPTY);

    event.data.key.keysym = 30;
    TEST_CHECK(jw_event_queue_push(queue, &event, &second_id) == JW_STATUS_OK);
    TEST_CHECK(second_id == 3);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 3 && output.data.key.keysym == 30);

    jw_event_queue_destroy(queue);
    return 1;
}

static int test_event_release_safety(void)
{
    jw_event_queue_t *queue;
    jw_event_queue_t *cross_device_queue;
    jw_input_event_t event;
    jw_input_event_t output;
    jw_event_queue_stats_t stats;
    uint64_t event_id;

    queue = jw_event_queue_create(2);
    TEST_CHECK(queue != NULL);

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_POINTER;
    event.data.pointer.x = 1;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_OK);
    event.data.pointer.x = 2;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_OK);
    event.data.pointer.x = 3;
    TEST_CHECK(jw_event_queue_push(queue, &event, &event_id) == JW_STATUS_OK);
    TEST_CHECK(event_id == 3);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 1 && output.data.pointer.x == 1);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 3 && output.data.pointer.x == 3);

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_KEY;
    event.data.key.keysym = 0x61U;
    event.data.key.pressed = 1;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_OK);
    event.data.key.keysym = 0x62U;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_OK);
    event.data.key.keysym = 0x61U;
    event.data.key.pressed = 0;
    TEST_CHECK(jw_event_queue_push(queue, &event, &event_id) == JW_STATUS_OK);
    TEST_CHECK(event_id == 6);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 6 && output.type == JW_INPUT_EVENT_RESET);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_EMPTY);

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_POINTER;
    event.data.pointer.x = 10;
    event.data.pointer.y = 20;
    event.data.pointer.buttons = 1;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_OK);
    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_KEY;
    event.data.key.keysym = 0x63U;
    event.data.key.pressed = 1;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) == JW_STATUS_OK);
    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_POINTER;
    event.data.pointer.x = 11;
    event.data.pointer.y = 21;
    event.data.pointer.buttons = 0;
    TEST_CHECK(jw_event_queue_push(queue, &event, &event_id) == JW_STATUS_OK);
    TEST_CHECK(event_id == 9);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 9 && output.type == JW_INPUT_EVENT_RESET);
    TEST_CHECK(jw_event_queue_pop(queue, &output) == JW_STATUS_EMPTY);

    TEST_CHECK(jw_event_queue_get_stats(queue, &stats) == JW_STATUS_OK);
    TEST_CHECK(stats.pending == 0 && stats.overflow_count == 5);
    TEST_CHECK(stats.next_event_id == 10);

    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_KEY;
    event.data.key.pressed = 2;
    TEST_CHECK(jw_event_queue_push(queue, &event, NULL) ==
        JW_STATUS_INVALID_ARGUMENT);

    jw_event_queue_destroy(queue);

    cross_device_queue = jw_event_queue_create(3);
    TEST_CHECK(cross_device_queue != NULL);
    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_KEY;
    event.data.key.keysym = 0x64U;
    event.data.key.pressed = 1;
    TEST_CHECK(jw_event_queue_push(cross_device_queue, &event, NULL) ==
        JW_STATUS_OK);
    event.data.key.pressed = 0;
    TEST_CHECK(jw_event_queue_push(cross_device_queue, &event, NULL) ==
        JW_STATUS_OK);
    memset(&event, 0, sizeof(event));
    event.type = JW_INPUT_EVENT_POINTER;
    event.data.pointer.buttons = 1;
    TEST_CHECK(jw_event_queue_push(cross_device_queue, &event, NULL) ==
        JW_STATUS_OK);
    event.data.pointer.buttons = 0;
    TEST_CHECK(jw_event_queue_push(cross_device_queue, &event, &event_id) ==
        JW_STATUS_OK);
    TEST_CHECK(event_id == 4);
    TEST_CHECK(jw_event_queue_pop(cross_device_queue, &output) == JW_STATUS_OK);
    TEST_CHECK(output.event_id == 4 && output.type == JW_INPUT_EVENT_RESET);
    TEST_CHECK(jw_event_queue_pop(cross_device_queue, &output) ==
        JW_STATUS_EMPTY);
    TEST_CHECK(jw_event_queue_get_stats(cross_device_queue, &stats) ==
        JW_STATUS_OK);
    TEST_CHECK(stats.pending == 0 && stats.overflow_count == 3);
    jw_event_queue_destroy(cross_device_queue);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s {frame|invalid|fifo|release}\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "frame") == 0) {
        return test_frame_stride_and_damage() ? 0 : 1;
    }
    if (strcmp(argv[1], "invalid") == 0) {
        return test_invalid_frames() ? 0 : 1;
    }
    if (strcmp(argv[1], "fifo") == 0) {
        return test_event_fifo() ? 0 : 1;
    }
    if (strcmp(argv[1], "release") == 0) {
        return test_event_release_safety() ? 0 : 1;
    }

    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
}
