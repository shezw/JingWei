/**
    -----------------------------------------------------------

    Project JingWei
    tests test_vnc_input.c    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#define _POSIX_C_SOURCE 200809L

#include <jingwei/vnc.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if JINGWEI_TEST_HAVE_LIBVNCSERVER
#include <rfb/rfbproto.h>
#endif

#define TEST_CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "check failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while (0)

typedef struct test_process_state {
    jw_vnc_backend_t *backend;
    pthread_mutex_t mutex;
    int stop;
    jw_status_t status;
} test_process_state_t;

static uint16_t test_read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t test_read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) |
        ((uint32_t)bytes[1] << 16) |
        ((uint32_t)bytes[2] << 8) |
        bytes[3];
}

static void test_write_be16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void test_write_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static int test_send_all(int socket_fd, const void *buffer, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)buffer;
    size_t sent = 0;

    while (sent < size) {
        ssize_t result = send(socket_fd, bytes + sent, size - sent, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return 0;
        }
        sent += (size_t)result;
    }
    return 1;
}

static int test_receive_all(int socket_fd, void *buffer, size_t size)
{
    uint8_t *bytes = (uint8_t *)buffer;
    size_t received = 0;

    while (received < size) {
        ssize_t result = recv(socket_fd, bytes + received, size - received, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return 0;
        }
        received += (size_t)result;
    }
    return 1;
}

static void test_sleep_milliseconds(long milliseconds)
{
    struct timespec delay;

    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = (milliseconds % 1000) * 1000000L;
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

static int test_set_socket_timeout(int socket_fd)
{
    struct timeval timeout;

    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    return setsockopt(
        socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
        setsockopt(
            socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
}

static int test_reserve_ipv4_port(int keep_open, int *socket_fd)
{
    struct sockaddr_in address;
    socklen_t address_size = (socklen_t)sizeof(address);
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = 0;
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(fd, (struct sockaddr *)&address, &address_size) != 0) {
        close(fd);
        return -1;
    }
    if (keep_open && listen(fd, 1) != 0) {
        close(fd);
        return -1;
    }

    if (keep_open) {
        *socket_fd = fd;
    } else {
        close(fd);
        *socket_fd = -1;
    }
    return (int)ntohs(address.sin_port);
}

static int test_connect_ipv4(int port)
{
    struct sockaddr_in address;
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0 || !test_set_socket_timeout(socket_fd)) {
        if (socket_fd >= 0) {
            close(socket_fd);
        }
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

static int test_ipv6_is_disabled(int port)
{
    struct sockaddr_in6 address;
    int socket_fd;
    int connected;

    socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return 1;
    }
    memset(&address, 0, sizeof(address));
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_loopback;
    address.sin6_port = htons((uint16_t)port);
    connected = connect(
        socket_fd, (struct sockaddr *)&address, sizeof(address)) == 0;
    close(socket_fd);
    return !connected;
}

static void *test_process_events(void *data)
{
    test_process_state_t *state = (test_process_state_t *)data;

    for (;;) {
        int stop;
        jw_status_t status;

        pthread_mutex_lock(&state->mutex);
        stop = state->stop;
        pthread_mutex_unlock(&state->mutex);
        if (stop) {
            break;
        }

        status = jw_vnc_backend_process_events(state->backend, 1000);
        if (status != JW_STATUS_OK) {
            pthread_mutex_lock(&state->mutex);
            state->status = status;
            state->stop = 1;
            pthread_mutex_unlock(&state->mutex);
            break;
        }
    }
    return NULL;
}

static int test_rfb_handshake(
    int socket_fd,
    uint16_t width,
    uint16_t height,
    const char *password,
    int expect_success)
{
    static const uint8_t protocol[] = "RFB 003.008\n";
    const size_t protocol_size = 12;
    uint8_t server_protocol[12];
    uint8_t security_count;
    uint8_t security_types[255];
    uint8_t selected_security = 2;
    uint8_t challenge[16];
    uint8_t security_result[4];
    uint8_t shared = 1;
    uint8_t server_init[24];
    uint32_t name_length;
    char *name;
    size_t index;
    int supports_vnc_auth = 0;
    int supports_none = 0;

    if (!test_receive_all(socket_fd, server_protocol, sizeof(server_protocol)) ||
        memcmp(server_protocol, protocol, protocol_size) != 0 ||
        !test_send_all(socket_fd, protocol, protocol_size) ||
        !test_receive_all(socket_fd, &security_count, 1) ||
        security_count == 0 ||
        !test_receive_all(socket_fd, security_types, security_count)) {
        return 0;
    }
    for (index = 0; index < security_count; ++index) {
        if (security_types[index] == 1) {
            supports_none = 1;
        }
        if (security_types[index] == selected_security) {
            supports_vnc_auth = 1;
        }
    }
    if (!supports_vnc_auth || supports_none || password == NULL ||
        !test_send_all(socket_fd, &selected_security, 1) ||
        !test_receive_all(socket_fd, challenge, sizeof(challenge))) {
        return 0;
    }
#if JINGWEI_TEST_HAVE_LIBVNCSERVER
    rfbEncryptBytes(challenge, (char *)password);
#else
    return 0;
#endif
    if (!test_send_all(socket_fd, challenge, sizeof(challenge)) ||
        !test_receive_all(socket_fd, security_result, sizeof(security_result)) ||
        (expect_success && test_read_be32(security_result) != 0) ||
        (!expect_success && test_read_be32(security_result) == 0)) {
        return 0;
    }
    if (!expect_success) {
        return 1;
    }
    if (
        !test_send_all(socket_fd, &shared, 1) ||
        !test_receive_all(socket_fd, server_init, sizeof(server_init)) ||
        test_read_be16(server_init) != width ||
        test_read_be16(server_init + 2) != height) {
        return 0;
    }

    name_length = test_read_be32(server_init + 20);
    if (name_length > 1024) {
        return 0;
    }
    name = (char *)malloc(name_length + 1U);
    if (name == NULL) {
        return 0;
    }
    if (!test_receive_all(socket_fd, name, name_length)) {
        free(name);
        return 0;
    }
    name[name_length] = '\0';
    supports_vnc_auth = strcmp(name, "JingWei network test") == 0;
    free(name);
    return supports_vnc_auth;
}

static int test_request_and_receive_pixels(
    int socket_fd,
    const uint8_t *expected_pixels,
    size_t expected_size)
{
    uint8_t set_format[20] = { 0 };
    uint8_t request[10] = { 0 };
    uint8_t update_header[4];
    uint8_t rectangle[12];
    uint8_t pixels[16];
    uint16_t rectangle_count;
    uint16_t index;
    int got_raw = 0;

    set_format[0] = 0;
    set_format[4] = 32;
    set_format[5] = 24;
    set_format[6] = 0;
    set_format[7] = 1;
    test_write_be16(set_format + 8, 255);
    test_write_be16(set_format + 10, 255);
    test_write_be16(set_format + 12, 255);
    set_format[14] = 16;
    set_format[15] = 8;
    set_format[16] = 0;

    request[0] = 3;
    request[1] = 0;
    test_write_be16(request + 2, 1);
    test_write_be16(request + 4, 1);
    test_write_be16(request + 6, 1);
    test_write_be16(request + 8, 1);
    if (!test_send_all(socket_fd, set_format, sizeof(set_format)) ||
        !test_send_all(socket_fd, request, sizeof(request)) ||
        !test_receive_all(socket_fd, update_header, sizeof(update_header)) ||
        update_header[0] != 0) {
        return 0;
    }

    rectangle_count = test_read_be16(update_header + 2);
    for (index = 0; index < rectangle_count; ++index) {
        uint16_t width;
        uint16_t height;
        uint32_t encoding;
        size_t pixel_size;

        if (!test_receive_all(socket_fd, rectangle, sizeof(rectangle))) {
            return 0;
        }
        width = test_read_be16(rectangle + 4);
        height = test_read_be16(rectangle + 6);
        encoding = test_read_be32(rectangle + 8);
        if (encoding != 0 || width != 1 || height != 1) {
            return 0;
        }
        pixel_size = (size_t)width * height * 4U;
        if (pixel_size != expected_size ||
            !test_receive_all(socket_fd, pixels, pixel_size)) {
            return 0;
        }
        if (test_read_be16(rectangle) == 1 &&
            test_read_be16(rectangle + 2) == 1 && width == 1 && height == 1 &&
            memcmp(pixels, expected_pixels, expected_size) == 0) {
            got_raw = 1;
        } else {
            size_t byte_index;

            fprintf(stderr,
                "unexpected raw rectangle x=%u y=%u w=%u h=%u pixels=",
                (unsigned int)test_read_be16(rectangle),
                (unsigned int)test_read_be16(rectangle + 2),
                (unsigned int)width,
                (unsigned int)height);
            for (byte_index = 0; byte_index < pixel_size; ++byte_index) {
                fprintf(stderr, "%02x", pixels[byte_index]);
            }
            fprintf(stderr, "\n");
        }
    }
    return got_raw;
}

static int test_send_input(int socket_fd)
{
    uint8_t pointer[6] = { 5, 1, 0, 1, 0, 1 };
    uint8_t pointer_release[6] = { 5, 0, 0, 1, 0, 1 };
    uint8_t key[8] = { 4, 1, 0, 0, 0, 0, 0, 0 };
    uint8_t key_release[8] = { 4, 0, 0, 0, 0, 0, 0, 0 };

    test_write_be32(key + 4, 0x61U);
    test_write_be32(key_release + 4, 0x61U);
    return test_send_all(socket_fd, pointer, sizeof(pointer)) &&
        test_send_all(socket_fd, pointer_release, sizeof(pointer_release)) &&
        test_send_all(socket_fd, key, sizeof(key)) &&
        test_send_all(socket_fd, key_release, sizeof(key_release));
}

static int test_wait_for_input(
    jw_surface_t *surface,
    jw_input_event_t *events,
    size_t event_count)
{
    size_t received = 0;
    int attempts;

    for (attempts = 0; attempts < 3000 && received < event_count; ++attempts) {
        jw_status_t status = jw_surface_poll_input(surface, &events[received]);
        if (status == JW_STATUS_OK) {
            received += 1;
        } else if (status == JW_STATUS_EMPTY) {
            test_sleep_milliseconds(1);
        } else {
            return 0;
        }
    }
    return received == event_count;
}

static int test_input_mapping(void)
{
    jw_surface_t *surface;
    jw_vnc_backend_t *backend;
    jw_vnc_config_t config = { 0 };
    jw_frame_t frame;
    jw_input_event_t event;
    jw_event_queue_stats_t stats;
    uint8_t pixels[64 * 48 * 4] = { 0 };
    uint64_t pointer_id;
    uint64_t key_id;
    uint64_t serial;

    surface = jw_surface_create(64, 48, 2);
    TEST_CHECK(surface != NULL);

    config.port = 15900;
    config.desktop_name = "JingWei test";
    config.password = NULL;
    backend = jw_vnc_backend_create(surface, &config);
    if (jw_vnc_backend_is_available()) {
        TEST_CHECK(backend != NULL);
        frame.pixels = pixels;
        frame.size = sizeof(pixels);
        frame.width = 64;
        frame.height = 48;
        frame.stride = 64 * 4;
        frame.format = JW_PIXEL_FORMAT_BGRA8888;
        TEST_CHECK(jw_vnc_backend_publish_frame(
            backend, &frame, NULL, 0, &serial) == JW_STATUS_OK);
        TEST_CHECK(serial == 1);
    } else {
        TEST_CHECK(backend == NULL);
    }

    TEST_CHECK(jw_vnc_enqueue_pointer(surface, 5, 12, 34, &pointer_id) ==
        JW_STATUS_OK);
    TEST_CHECK(jw_vnc_enqueue_key(surface, 1, 0xff0dU, &key_id) ==
        JW_STATUS_OK);
    TEST_CHECK(pointer_id == 1 && key_id == 2);
    TEST_CHECK(jw_surface_poll_input(surface, &event) == JW_STATUS_OK);
    TEST_CHECK(event.type == JW_INPUT_EVENT_POINTER);
    TEST_CHECK(event.event_id == pointer_id);
    TEST_CHECK(event.data.pointer.buttons == 5);
    TEST_CHECK(event.data.pointer.x == 12 && event.data.pointer.y == 34);

    TEST_CHECK(jw_surface_poll_input(surface, &event) == JW_STATUS_OK);
    TEST_CHECK(event.type == JW_INPUT_EVENT_KEY);
    TEST_CHECK(event.event_id == key_id);
    TEST_CHECK(event.data.key.pressed == 1);
    TEST_CHECK(event.data.key.keysym == 0xff0dU);
    TEST_CHECK(jw_vnc_enqueue_key(surface, 0, 0xff0dU, NULL) ==
        JW_STATUS_OK);
    TEST_CHECK(jw_surface_poll_input(surface, &event) == JW_STATUS_OK);
    TEST_CHECK(event.type == JW_INPUT_EVENT_KEY && event.data.key.pressed == 0);

    TEST_CHECK(jw_vnc_enqueue_pointer(surface, 0, -1, 10, NULL) ==
        JW_STATUS_INVALID_ARGUMENT);
    TEST_CHECK(jw_vnc_enqueue_pointer(surface, 0, 64, 10, NULL) ==
        JW_STATUS_INVALID_ARGUMENT);
    TEST_CHECK(jw_vnc_enqueue_key(surface, 2, 1, NULL) ==
        JW_STATUS_INVALID_ARGUMENT);
    TEST_CHECK(jw_surface_get_input_stats(surface, &stats) == JW_STATUS_OK);
    TEST_CHECK(stats.overflow_count == 0);

    jw_vnc_backend_destroy(backend);
    jw_surface_destroy(surface);
    return 1;
}

static int test_password_validation(void)
{
    jw_surface_t *surface;
    jw_vnc_backend_t *backend;
    jw_vnc_config_t config = { 0 };
    char non_ascii[] = { (char)0xc3, (char)0xa9, '\0' };

    TEST_CHECK(jw_vnc_backend_is_available());
    surface = jw_surface_create(2, 2, 2);
    TEST_CHECK(surface != NULL);
    config.desktop_name = "JingWei password validation";

    config.password = "a";
    backend = jw_vnc_backend_create(surface, &config);
    TEST_CHECK(backend != NULL);
    jw_vnc_backend_destroy(backend);

    config.password = "12345678";
    backend = jw_vnc_backend_create(surface, &config);
    TEST_CHECK(backend != NULL);
    jw_vnc_backend_destroy(backend);

    config.password = "";
    TEST_CHECK(jw_vnc_backend_create(surface, &config) == NULL);
    config.password = "123456789";
    TEST_CHECK(jw_vnc_backend_create(surface, &config) == NULL);
    config.password = "bad pass";
    TEST_CHECK(jw_vnc_backend_create(surface, &config) == NULL);
    config.password = non_ascii;
    TEST_CHECK(jw_vnc_backend_create(surface, &config) == NULL);

    jw_surface_destroy(surface);
    return 1;
}

static int test_vnc_network(void)
{
    static const uint8_t pixels[16] = {
        0x11, 0x22, 0x33, 0xff,
        0x44, 0x55, 0x66, 0xff,
        0x77, 0x88, 0x99, 0xff,
        0xaa, 0xbb, 0xcc, 0xff
    };
    jw_surface_t *surface = NULL;
    jw_vnc_backend_t *backend = NULL;
    jw_vnc_backend_t *blocked_backend = NULL;
    jw_vnc_config_t config = { 0 };
    jw_frame_t frame;
    jw_input_event_t events[4];
    uint8_t snapshot[16];
    test_process_state_t process_state;
    pthread_t process_thread;
    int process_mutex_initialized = 0;
    int process_thread_started = 0;
    int backend_started = 0;
    int socket_fd = -1;
    int reserved_socket = -1;
    int port;
    int blocked_port;
    int result = 0;
    char password[] = "testpass";

    if (!jw_vnc_backend_is_available()) {
        fprintf(stderr,
            "network test requires a real LibVNCServer backend\n");
        return 0;
    }

#define NETWORK_CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "network check failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            goto cleanup; \
        } \
    } while (0)

    surface = jw_surface_create(2, 2, 16);
    NETWORK_CHECK(surface != NULL);
    port = test_reserve_ipv4_port(0, &reserved_socket);
    NETWORK_CHECK(port > 0);

    config.port = port;
    config.desktop_name = "JingWei network test";
    config.password = password;
    backend = jw_vnc_backend_create(surface, &config);
    NETWORK_CHECK(backend != NULL);
    password[0] = 'X';

    frame.pixels = pixels;
    frame.size = sizeof(pixels);
    frame.width = 2;
    frame.height = 2;
    frame.stride = 8;
    frame.format = JW_PIXEL_FORMAT_BGRA8888;
    NETWORK_CHECK(jw_vnc_backend_publish_frame(
        backend, &frame, &(jw_damage_t){ 0, 0, 1, 1 }, 1, NULL) ==
        JW_STATUS_OK);
    NETWORK_CHECK(jw_surface_copy_front_buffer(
        surface, snapshot, sizeof(snapshot), 8, NULL) == JW_STATUS_OK);
    NETWORK_CHECK(memcmp(snapshot, pixels, sizeof(snapshot)) == 0);
    NETWORK_CHECK(jw_vnc_backend_start(backend) == JW_STATUS_OK);
    backend_started = 1;
    NETWORK_CHECK(test_ipv6_is_disabled(port));

    memset(&process_state, 0, sizeof(process_state));
    process_state.backend = backend;
    process_state.status = JW_STATUS_OK;
    NETWORK_CHECK(pthread_mutex_init(&process_state.mutex, NULL) == 0);
    process_mutex_initialized = 1;
    NETWORK_CHECK(pthread_create(
        &process_thread, NULL, test_process_events, &process_state) == 0);
    process_thread_started = 1;

    socket_fd = test_connect_ipv4(port);
    NETWORK_CHECK(socket_fd >= 0);
    NETWORK_CHECK(test_rfb_handshake(
        socket_fd, 2, 2, "wrongpas", 0));
    close(socket_fd);
    socket_fd = test_connect_ipv4(port);
    NETWORK_CHECK(socket_fd >= 0);
    NETWORK_CHECK(test_rfb_handshake(
        socket_fd, 2, 2, "testpass", 1));
    NETWORK_CHECK(test_request_and_receive_pixels(
        socket_fd, pixels + 12, 4));
    NETWORK_CHECK(test_send_input(socket_fd));
    NETWORK_CHECK(test_wait_for_input(surface, events, 4));
    NETWORK_CHECK(events[0].type == JW_INPUT_EVENT_POINTER &&
        events[0].data.pointer.buttons == 1 &&
        events[0].data.pointer.x == 1 && events[0].data.pointer.y == 1);
    NETWORK_CHECK(events[1].type == JW_INPUT_EVENT_POINTER &&
        events[1].data.pointer.buttons == 0);
    NETWORK_CHECK(events[2].type == JW_INPUT_EVENT_KEY &&
        events[2].data.key.pressed == 1 && events[2].data.key.keysym == 0x61U);
    NETWORK_CHECK(events[3].type == JW_INPUT_EVENT_KEY &&
        events[3].data.key.pressed == 0 && events[3].data.key.keysym == 0x61U);

    close(socket_fd);
    socket_fd = -1;
    pthread_mutex_lock(&process_state.mutex);
    process_state.stop = 1;
    pthread_mutex_unlock(&process_state.mutex);
    NETWORK_CHECK(pthread_join(process_thread, NULL) == 0);
    process_thread_started = 0;
    NETWORK_CHECK(process_state.status == JW_STATUS_OK);
    NETWORK_CHECK(jw_vnc_backend_stop(backend) == JW_STATUS_OK);
    backend_started = 0;
    jw_vnc_backend_destroy(backend);
    backend = NULL;

    blocked_port = test_reserve_ipv4_port(1, &reserved_socket);
    NETWORK_CHECK(blocked_port > 0 && reserved_socket >= 0);
    config.port = blocked_port;
    config.desktop_name = "JingWei blocked port test";
    config.password = "testpass";
    blocked_backend = jw_vnc_backend_create(surface, &config);
    NETWORK_CHECK(blocked_backend != NULL);
    NETWORK_CHECK(jw_vnc_backend_start(blocked_backend) == JW_STATUS_IO_ERROR);
    result = 1;

cleanup:
    if (socket_fd >= 0) {
        close(socket_fd);
    }
    if (process_thread_started) {
        pthread_mutex_lock(&process_state.mutex);
        process_state.stop = 1;
        pthread_mutex_unlock(&process_state.mutex);
        (void)pthread_join(process_thread, NULL);
    }
    if (reserved_socket >= 0) {
        close(reserved_socket);
    }
    if (backend_started) {
        (void)jw_vnc_backend_stop(backend);
    }
    jw_vnc_backend_destroy(blocked_backend);
    jw_vnc_backend_destroy(backend);
    if (process_mutex_initialized) {
        pthread_mutex_destroy(&process_state.mutex);
    }
    jw_surface_destroy(surface);
#undef NETWORK_CHECK
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s {mapping|password|network}\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "mapping") == 0) {
        return test_input_mapping() ? 0 : 1;
    }
    if (strcmp(argv[1], "password") == 0) {
        return test_password_validation() ? 0 : 1;
    }
    if (strcmp(argv[1], "network") == 0) {
        return test_vnc_network() ? 0 : 1;
    }
    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
}
