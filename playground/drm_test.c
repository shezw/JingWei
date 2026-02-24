/**
    -----------------------------------------------------------

 	Project JingWei
 	playground drm_test.c    2026/02/24
 	
 	@link    : https://github.com/shezw/jingwei
 	@author	 : shezw
 	@email	 : hello@shezw.com

    -----------------------------------------------------------
*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	uint8_t *map;
	uint32_t fb_id;
};

struct buffer_object buf;
static int drm_fd;
static uint32_t conn_id;
static uint32_t crtc_id;
static drmModeModeInfo mode;

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {};
	struct drm_mode_map_dumb map = {};
	struct drm_mode_destroy_dumb destroy = {};
	int ret;

	// Create dumb buffer
	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		fprintf(stderr, "cannot create dumb buffer (%d): %m\n", errno);
		return -errno;
	}
	bo->stride = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;

	// Create framebuffer object
	ret = drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->stride, bo->handle, &bo->fb_id);
	if (ret) {
		fprintf(stderr, "cannot create framebuffer (%d): %m\n", errno);
		goto err_destroy;
	}

	// Prepare buffer for memory mapping
	map.handle = bo->handle;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
	if (ret) {
		fprintf(stderr, "cannot map dumb buffer (%d): %m\n", errno);
		goto err_fb;
	}

	// Perform actual memory mapping
	bo->map = mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
	if (bo->map == MAP_FAILED) {
		fprintf(stderr, "cannot mmap dumb buffer (%d): %m\n", errno);
		ret = -errno;
		goto err_fb;
	}

	// Clear the buffer to black initially
	memset(bo->map, 0, bo->size);

	return 0;

err_fb:
	drmModeRmFB(fd, bo->fb_id);
err_destroy:
	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
	return ret;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);
	munmap(bo->map, bo->size);
	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static int modeset_find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, uint32_t *crtc_out)
{
	drmModeEncoder *enc;
	unsigned int i, j;
	int32_t crtc_id = -1;

	// First try the currently connected encoder+crtc
	if (conn->encoder_id) {
		enc = drmModeGetEncoder(fd, conn->encoder_id);
		if (enc) {
			if (enc->crtc_id) {
				crtc_id = enc->crtc_id;
				drmModeFreeEncoder(enc);
				*crtc_out = crtc_id;
				return 0;
			}
			drmModeFreeEncoder(enc);
		}
	}

	// If not, find a suitable encoder/crtc pair
	for (i = 0; i < conn->count_encoders; ++i) {
		enc = drmModeGetEncoder(fd, conn->encoders[i]);
		if (!enc)
			continue;

		for (j = 0; j < res->count_crtcs; ++j) {
			if (enc->possible_crtcs & (1 << j)) {
				crtc_id = res->crtcs[j];
				*crtc_out = crtc_id;
				drmModeFreeEncoder(enc);
				return 0;
			}
		}
		drmModeFreeEncoder(enc);
	}

	return -ENOENT;
}

static int modeset_setup_dev(int fd, drmModeRes **res, drmModeConnector **conn, uint32_t *crtc_out)
{
	*res = drmModeGetResources(fd);
	if (!*res) {
		fprintf(stderr, "cannot retrieve DRM resources (%d): %m\n", errno);
		return -errno;
	}

	for (int i = 0; i < (*res)->count_connectors; i++) {
		*conn = drmModeGetConnector(fd, (*res)->connectors[i]);
		if (!*conn)
			continue;

		if ((*conn)->connection == DRM_MODE_CONNECTED && (*conn)->count_modes > 0) {
			if (modeset_find_crtc(fd, *res, *conn, crtc_out) == 0) {
				return 0;
			}
		}

		drmModeFreeConnector(*conn);
	}

	fprintf(stderr, "cannot find suitable connector/crtc\n");
	drmModeFreeResources(*res);
	return -ENOENT;
}

static void fill_buffer(struct buffer_object *bo, uint8_t r, uint8_t g, uint8_t b)
{
	// Assuming 32-bit (XRGB8888 or ARGB8888)
	// Byte order is typically B G R A (little endian)
	uint32_t color = (r << 16) | (g << 8) | b;
	for (uint32_t i = 0; i < bo->size / 4; i++) {
		((uint32_t*)bo->map)[i] = color;
	}

    // Important: For virtual drivers (virtio, vmwgfx, qxl) or USB diaplay links,
    // we must manually flush the changed area using the DIRTYFB ioctl.
    // Without this, the screen may remain black even if memory is written.
    drmModeClip clip = {
        .x1 = 0,
        .y1 = 0,
        .x2 = bo->width,
        .y2 = bo->height,
    };
    drmModeDirtyFB(drm_fd, bo->fb_id, &clip, 1);
}

int main(int argc, char **argv)
{
	int ret;
	drmModeRes *res;
	drmModeConnector *conn;
	uint32_t crtc;
	
	// Open DRM device
	drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (drm_fd < 0) {
		perror("cannot open /dev/dri/card0");
		return 1;
	}

	// Setup DRM resources
	ret = modeset_setup_dev(drm_fd, &res, &conn, &crtc);
	if (ret) {
		close(drm_fd);
		return 1;
	}

	// Use the first valid mode
	conn_id = conn->connector_id;
	crtc_id = crtc;
	mode = conn->modes[0];
	
	printf("Using mode: %s %dx%d\n", mode.name, mode.hdisplay, mode.vdisplay);

	// Create dumb buffer matching the mode
	buf.width = mode.hdisplay;
	buf.height = mode.vdisplay;
	ret = modeset_create_fb(drm_fd, &buf);
	if (ret) {
		goto cleanup;
	}

	// Set mode!
	ret = drmModeSetCrtc(drm_fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &mode);
	if (ret) {
		fprintf(stderr, "cannot set CRTC for connector %u (%d): %m\n", conn_id, errno);
		goto cleanup_fb;
	}

	// Step 1: Red
	printf("Displaying RED...\n");
	fill_buffer(&buf, 255, 0, 0);
	sleep(3);

	// Step 2: Green
	printf("Displaying GREEN...\n");
	fill_buffer(&buf, 0, 255, 0);
	sleep(3);

    // Step 3: Blue
	printf("Displaying BLUE...\n");
	fill_buffer(&buf, 0, 0, 255);
	sleep(3);
    
    printf("Test finished.\n");

cleanup_fb:
	modeset_destroy_fb(drm_fd, &buf);
cleanup:
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(drm_fd);

	return ret;
}
