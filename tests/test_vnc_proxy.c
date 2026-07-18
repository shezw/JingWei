/*
    JingWei
    tests test_vnc_proxy.c    2026-07-18

     ______     __  __     ______     ______     __     __
    /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
    \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
     \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
      \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com

    @link    : https://github.com/shezw/JingWei
    @author  : shezw
    @email   : hello@shezw.com
*/

#define _POSIX_C_SOURCE 200809L

#include <jingwei.h>
#include <jingwei/vnc.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
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

typedef struct test_client_state {
    pthread_mutex_t mutex;
    int port;
    int done;
    int success;
} test_client_state_t;

typedef struct test_event_state {
    jw_event_t events[8];
    size_t event_count;
} test_event_state_t;

static int test_send_all(int socket_fd, const void *buffer, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)buffer;
    size_t sent = 0U;

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
    size_t received = 0U;

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

static uint32_t test_read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) |
        ((uint32_t)bytes[1] << 16) |
        ((uint32_t)bytes[2] << 8) |
        bytes[3];
}

static void test_write_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static int test_connect(int port)
{
    struct sockaddr_in address;
    struct timeval timeout;
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1;
    }
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(
            socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(
            socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        close(socket_fd);
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

static int test_authenticate(int socket_fd)
{
    static const uint8_t protocol[] = "RFB 003.008\n";
    uint8_t server_protocol[12];
    uint8_t security_count;
    uint8_t security_types[255];
    uint8_t selected_security = 2U;
    uint8_t challenge[16];
    uint8_t result[4];
    uint8_t shared = 1U;
    uint8_t server_init[24];
    char *name;
    uint32_t name_length;
    size_t index;
    int supports_auth = 0;

    if (!test_receive_all(socket_fd, server_protocol, sizeof(server_protocol)) ||
        memcmp(server_protocol, protocol, sizeof(server_protocol)) != 0 ||
        !test_send_all(socket_fd, protocol, sizeof(server_protocol)) ||
        !test_receive_all(socket_fd, &security_count, 1U) ||
        security_count == 0U ||
        !test_receive_all(socket_fd, security_types, security_count)) {
        return 0;
    }
    for (index = 0U; index < security_count; ++index) {
        if (security_types[index] == selected_security) {
            supports_auth = 1;
        }
        if (security_types[index] == 1U) {
            return 0;
        }
    }
    if (!supports_auth ||
        !test_send_all(socket_fd, &selected_security, 1U) ||
        !test_receive_all(socket_fd, challenge, sizeof(challenge))) {
        return 0;
    }
#if JINGWEI_TEST_HAVE_LIBVNCSERVER
    rfbEncryptBytes(challenge, "testpass");
#else
    return 0;
#endif
    if (!test_send_all(socket_fd, challenge, sizeof(challenge)) ||
        !test_receive_all(socket_fd, result, sizeof(result)) ||
        test_read_be32(result) != 0U ||
        !test_send_all(socket_fd, &shared, 1U) ||
        !test_receive_all(socket_fd, server_init, sizeof(server_init))) {
        return 0;
    }
    name_length = test_read_be32(server_init + 20);
    if (name_length > 1024U) {
        return 0;
    }
    name = (char *)malloc((size_t)name_length + 1U);
    if (!name) {
        return 0;
    }
    if (!test_receive_all(socket_fd, name, name_length)) {
        free(name);
        return 0;
    }
    name[name_length] = '\0';
    supports_auth = strcmp(name, "JingWei VNC proxy test") == 0;
    free(name);
    return supports_auth;
}

static int test_send_input(int socket_fd)
{
    uint8_t pointer_down[6] = { 5U, 1U, 0U, 1U, 0U, 1U };
    uint8_t pointer_up[6] = { 5U, 0U, 0U, 1U, 0U, 1U };
    uint8_t key_down[8] = { 4U, 1U, 0U, 0U, 0U, 0U, 0U, 0U };
    uint8_t key_up[8] = { 4U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };

    test_write_be32(key_down + 4, 0x61U);
    test_write_be32(key_up + 4, 0x61U);
    return test_send_all(socket_fd, pointer_down, sizeof(pointer_down)) &&
        test_send_all(socket_fd, pointer_up, sizeof(pointer_up)) &&
        test_send_all(socket_fd, key_down, sizeof(key_down)) &&
        test_send_all(socket_fd, key_up, sizeof(key_up));
}

static void *test_client_main(void *data)
{
    test_client_state_t *state = (test_client_state_t *)data;
    int socket_fd = test_connect(state->port);
    int success = socket_fd >= 0 && test_authenticate(socket_fd) &&
        test_send_input(socket_fd);

    if (socket_fd >= 0) {
        close(socket_fd);
    }
    pthread_mutex_lock(&state->mutex);
    state->success = success;
    state->done = 1;
    pthread_mutex_unlock(&state->mutex);
    return NULL;
}

static void test_capture_event(
    jw_context_t *context,
    jw_display_t *display,
    const jw_event_t *event,
    void *user_data)
{
    test_event_state_t *state = (test_event_state_t *)user_data;

    (void)context;
    (void)display;
    if (state->event_count < sizeof(state->events) / sizeof(state->events[0])) {
        state->events[state->event_count++] = *event;
    }
}

static void test_sleep_millisecond(void)
{
    struct timespec delay = { 0, 1000000L };

    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

static int test_reserve_port(void)
{
    struct sockaddr_in address;
    socklen_t address_size = (socklen_t)sizeof(address);
    int socket_fd;
    int port;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = 0;
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(
            socket_fd, (struct sockaddr *)&address, &address_size) != 0) {
        close(socket_fd);
        return -1;
    }
    port = (int)ntohs(address.sin_port);
    close(socket_fd);
    return port;
}

static int test_vnc_proxy_lifecycle(void)
{
    jw_vnc_config_t config = { 0 };
    jw_context_t *context;
    jw_proxy_t *proxy;
    jw_proxy_t *unowned_proxy;
    jw_display_t *display;
    jw_event_manager_t *event_manager;
    jw_buffer_t *buffer;
    jw_buffer_t *unsupported_buffer;
    jw_rect_t damage = { 0, 0, 1, 1 };
    test_client_state_t client_state;
    test_event_state_t event_state;
    pthread_t client_thread;
    uint32_t unsupported_pixel = 0U;
    char password[] = "testpass";
    int port;
    int attempt;
    int client_done = 0;

    if (!jw_vnc_backend_is_available()) {
        TEST_CHECK(JW_PIXEL_FORMAT_INVALID == 0);
        TEST_CHECK(JW_PIXEL_FORMAT_ARGB8888 == 1);
        TEST_CHECK(JW_PIXEL_FORMAT_XRGB8888 == 2);
        TEST_CHECK(JW_PIXEL_FORMAT_BGRA8888 == 3);
        TEST_CHECK(JW_PIXEL_FORMAT_BGRX8888 == 4);
        TEST_CHECK(jw_proxy_create_vnc(2, 2, &config) == NULL);
        return 1;
    }

    TEST_CHECK(JW_PIXEL_FORMAT_INVALID == 0);
    TEST_CHECK(JW_PIXEL_FORMAT_ARGB8888 == 1);
    TEST_CHECK(JW_PIXEL_FORMAT_XRGB8888 == 2);
    TEST_CHECK(JW_PIXEL_FORMAT_BGRA8888 == 3);
    TEST_CHECK(JW_PIXEL_FORMAT_BGRX8888 == 4);

    port = test_reserve_port();
    TEST_CHECK(port > 0);
    config.port = port;
    config.desktop_name = "JingWei VNC proxy test";
    config.password = password;
    unowned_proxy = jw_proxy_create_vnc(2, 2, &config);
    TEST_CHECK(unowned_proxy != NULL);
    jw_proxy_destroy(unowned_proxy);
    proxy = jw_proxy_create_vnc(2, 2, &config);
    TEST_CHECK(proxy != NULL);
    password[0] = 'X';

    display = jw_display_create(2, 2, proxy);
    TEST_CHECK(display != NULL);
    buffer = jw_buffer_create(2, 2);
    TEST_CHECK(buffer != NULL);
    TEST_CHECK(jw_buffer_fill(buffer, 0xff112233U) == 0);
    TEST_CHECK(((const uint32_t *)jw_buffer_const_data(buffer))[0] ==
        0xff112233U);
    TEST_CHECK(jw_display_present_buffer(display, buffer) == 0);
    TEST_CHECK(jw_display_present_buffer_rects(display, buffer, &damage, 1) == 0);
    damage.w = 3;
    TEST_CHECK(jw_display_present_buffer_rects(display, buffer, &damage, 1) != 0);
    damage.w = 1;

    unsupported_buffer = jw_buffer_wrap_pixels(
        1,
        1,
        (int)sizeof(unsupported_pixel),
        JW_PIXEL_FORMAT_BGRA8888,
        &unsupported_pixel,
        NULL,
        NULL);
    TEST_CHECK(unsupported_buffer != NULL);
    TEST_CHECK(jw_buffer_fill(unsupported_buffer, 0xff112233U) == 0);
    TEST_CHECK(unsupported_pixel == 0x332211ffU);
    TEST_CHECK(jw_display_present_buffer(display, unsupported_buffer) != 0);
    jw_buffer_destroy(unsupported_buffer);
    jw_buffer_destroy(buffer);

    event_manager = jw_event_manager_create_mouse(proxy);
    TEST_CHECK(event_manager != NULL);
    TEST_CHECK(jw_display_bind_event_manager(display, event_manager) == 0);
    context = jw_context_create();
    TEST_CHECK(context != NULL);
    TEST_CHECK(jw_context_register_display(context, display) > 0);
    memset(&event_state, 0, sizeof(event_state));
    jw_context_set_event_callback(context, test_capture_event, &event_state);
    memset(&client_state, 0, sizeof(client_state));
    client_state.port = port;
    TEST_CHECK(pthread_mutex_init(&client_state.mutex, NULL) == 0);
    TEST_CHECK(pthread_create(
        &client_thread, NULL, test_client_main, &client_state) == 0);
    for (attempt = 0; attempt < 5000; ++attempt) {
        TEST_CHECK(jw_context_poll(context, 0) >= 0);
        pthread_mutex_lock(&client_state.mutex);
        client_done = client_state.done;
        pthread_mutex_unlock(&client_state.mutex);
        if (client_done && event_state.event_count >= 5U) {
            break;
        }
        test_sleep_millisecond();
    }
    TEST_CHECK(pthread_join(client_thread, NULL) == 0);
    TEST_CHECK(client_state.success);
    pthread_mutex_destroy(&client_state.mutex);
    TEST_CHECK(event_state.event_count == 5U);
    TEST_CHECK(event_state.events[0].type == JW_EVENT_MOUSE_MOVE);
    TEST_CHECK(event_state.events[0].data.mouse_move.x == 1 &&
        event_state.events[0].data.mouse_move.y == 1);
    TEST_CHECK(event_state.events[1].type == JW_EVENT_MOUSE_KEY &&
        event_state.events[1].data.mouse_key.button == JW_MOUSE_LEFT &&
        event_state.events[1].data.mouse_key.state == JW_BUTTON_DOWN);
    TEST_CHECK(event_state.events[2].type == JW_EVENT_MOUSE_KEY &&
        event_state.events[2].data.mouse_key.state == JW_BUTTON_UP);
    TEST_CHECK(event_state.events[3].type == JW_EVENT_KEY &&
        event_state.events[3].data.key.key == 0x61 &&
        event_state.events[3].data.key.state == JW_KEY_DOWN);
    TEST_CHECK(event_state.events[4].type == JW_EVENT_KEY &&
        event_state.events[4].data.key.state == JW_KEY_UP);
    jw_context_destroy(context);
    return 1;
}

int main(void)
{
    return test_vnc_proxy_lifecycle() ? 0 : 1;
}
