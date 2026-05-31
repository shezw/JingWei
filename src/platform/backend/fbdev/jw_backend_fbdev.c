#include "jw_internal.h"

#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct jw_fbdev_impl {
    char *device;
    int fd;
    unsigned char *map;
    long map_size;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
} jw_fbdev_impl_t;

static char *jw_fbdev_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        return NULL;
    }

    len = strlen(text) + 1u;
    copy = (char *)malloc(len);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len);
    return copy;
}

static uint32_t scale_channel(uint32_t value, uint32_t length)
{
    if (length >= 8) {
        return value << (length - 8);
    }
    return value >> (8 - length);
}

static uint32_t fbdev_pack_argb(const struct fb_var_screeninfo *vinfo, uint32_t argb)
{
    uint32_t a = (argb >> 24) & 0xffu;
    uint32_t r = (argb >> 16) & 0xffu;
    uint32_t g = (argb >> 8) & 0xffu;
    uint32_t b = argb & 0xffu;
    uint32_t pixel = 0;

    pixel |= scale_channel(r, vinfo->red.length) << vinfo->red.offset;
    pixel |= scale_channel(g, vinfo->green.length) << vinfo->green.offset;
    pixel |= scale_channel(b, vinfo->blue.length) << vinfo->blue.offset;
    if (vinfo->transp.length > 0) {
        pixel |= scale_channel(a, vinfo->transp.length) << vinfo->transp.offset;
    }
    return pixel;
}

static int jw_fbdev_init(jw_proxy_t *proxy)
{
    jw_fbdev_impl_t *impl = (jw_fbdev_impl_t *)proxy->impl;
    const char *device = impl->device ? impl->device : "/dev/fb0";

    impl->fd = open(device, O_RDWR);
    if (impl->fd < 0) {
        return -errno;
    }

    if (ioctl(impl->fd, FBIOGET_VSCREENINFO, &impl->vinfo) != 0 ||
        ioctl(impl->fd, FBIOGET_FSCREENINFO, &impl->finfo) != 0) {
        close(impl->fd);
        impl->fd = -1;
        return -errno;
    }

    proxy->width = (int)impl->vinfo.xres;
    proxy->height = (int)impl->vinfo.yres;
    impl->map_size = (long)impl->vinfo.yres_virtual * (long)impl->finfo.line_length;
    impl->map = (unsigned char *)mmap(0,
                                      (size_t)impl->map_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED,
                                      impl->fd,
                                      0);
    if (impl->map == MAP_FAILED) {
        impl->map = NULL;
        close(impl->fd);
        impl->fd = -1;
        return -errno;
    }

    return 0;
}

static void jw_fbdev_deinit(jw_proxy_t *proxy)
{
    jw_fbdev_impl_t *impl;

    if (!proxy || !proxy->impl) {
        return;
    }

    impl = (jw_fbdev_impl_t *)proxy->impl;
    if (impl->map) {
        munmap(impl->map, (size_t)impl->map_size);
        impl->map = NULL;
    }
    if (impl->fd >= 0) {
        close(impl->fd);
        impl->fd = -1;
    }
    free(impl->device);
    impl->device = NULL;
}

static int jw_fbdev_commit(jw_proxy_t *proxy, const jw_buffer_t *buffer)
{
    jw_fbdev_impl_t *impl;
    int bytes_per_pixel;
    int x;
    int y;

    if (!proxy || !proxy->impl || !buffer || !buffer->pixels) {
        return -1;
    }

    impl = (jw_fbdev_impl_t *)proxy->impl;
    if (!impl->map) {
        return -1;
    }

    bytes_per_pixel = (int)impl->vinfo.bits_per_pixel / 8;
    for (y = 0; y < buffer->height && y < (int)impl->vinfo.yres; ++y) {
        for (x = 0; x < buffer->width && x < (int)impl->vinfo.xres; ++x) {
            long offset = (long)(x + (int)impl->vinfo.xoffset) * bytes_per_pixel +
                          (long)(y + (int)impl->vinfo.yoffset) * (long)impl->finfo.line_length;
            const uint32_t *row = (const uint32_t *)((const unsigned char *)buffer->pixels + ((size_t)y * (size_t)buffer->stride));
            uint32_t argb = jw_unpack_argb_from_format(row[x], buffer->format);
            uint32_t pixel = fbdev_pack_argb(&impl->vinfo, argb);

            if (bytes_per_pixel == 4) {
                *((uint32_t *)(impl->map + offset)) = pixel;
            } else if (bytes_per_pixel == 2) {
                *((uint16_t *)(impl->map + offset)) = (uint16_t)pixel;
            } else if (bytes_per_pixel == 3) {
                impl->map[offset + 0] = (unsigned char)(pixel & 0xffu);
                impl->map[offset + 1] = (unsigned char)((pixel >> 8) & 0xffu);
                impl->map[offset + 2] = (unsigned char)((pixel >> 16) & 0xffu);
            }
        }
    }

    return 0;
}

static int jw_fbdev_poll_event(jw_proxy_t *proxy, jw_event_t *event)
{
    (void)proxy;
    (void)event;
    return 0;
}

static const jw_proxy_ops_t JW_FBDEV_OPS = {
    jw_fbdev_init,
    jw_fbdev_deinit,
    jw_fbdev_commit,
    NULL,
    NULL,
    jw_fbdev_poll_event
};

jw_proxy_t *jw_proxy_create_fbdev(const char *device, int width, int height, const char *title)
{
    jw_fbdev_impl_t *impl;
    (void)title;

    impl = (jw_fbdev_impl_t *)calloc(1, sizeof(*impl));
    if (!impl) {
        return NULL;
    }

    impl->fd = -1;
    if (device) {
        impl->device = jw_fbdev_strdup(device);
    }

    return jw_proxy_alloc(&JW_FBDEV_OPS, width > 0 ? width : 800, height > 0 ? height : 480, impl);
}

#else

jw_proxy_t *jw_proxy_create_fbdev(const char *device, int width, int height, const char *title)
{
    (void)device;
    return jw_proxy_create_sdl(width > 0 ? width : 800,
                               height > 0 ? height : 480,
                               title ? title : "JingWei fbdev SDL simulation");
}

#endif
