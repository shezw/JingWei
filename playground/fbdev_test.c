/**
    -----------------------------------------------------------

 	Project JingWei
 	playground fbdev_test.c    2026/02/24
 	
 	@link    : https://github.com/shezw/jingwei
 	@author	 : shezw
 	@email	 : hello@shezw.com

    -----------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

/**
 * JingWei Experiment: fbdev yellow screen test
 * Draws 255, 255, 0 (Yellow) to the framebuffer /dev/fb0
 * Strictly C implementation as per architecture requirements.
 */

int main() {
    const char* fb_path = "/dev/fb0";
    int fb_fd = open(fb_path, O_RDWR);
    if (fb_fd == -1) {
        perror("Error: cannot open framebuffer device");
        return 1;
    }
    printf("Successfully opened %s\n", fb_path);

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error: reading variable information");
        close(fb_fd);
        return 1;
    }

    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error: reading fixed information");
        close(fb_fd);
        return 1;
    }

    long screensize = vinfo.yres_virtual * finfo.line_length;
    uint8_t* fbp = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((intptr_t)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        close(fb_fd);
        return 1;
    }

    printf("Display info: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // Drawing Yellow (255, 255, 0)
    // R=255, G=255, B=0
    
    // Determine offsets
    int r_off = vinfo.red.offset;
    int g_off = vinfo.green.offset;
    int b_off = vinfo.blue.offset;
    int a_off = vinfo.transp.offset; // alpha often unused but good to know

    printf("Red offset: %d, Green offset: %d, Blue offset: %d\n", r_off, g_off, b_off);

    for (uint32_t y = 0; y < vinfo.yres; y++) {
        for (uint32_t x = 0; x < vinfo.xres; x++) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (y + vinfo.yoffset) * finfo.line_length;

            if (vinfo.bits_per_pixel == 32) {
                // Assuming typical 32-bit layout: AARRGGBB or similar.
                uint32_t pixel = 0;
                pixel |= (255 << r_off);
                pixel |= (255 << g_off);
                pixel |= (0 << b_off);
                // Set alpha to opaque just in case
                if (vinfo.transp.length > 0) {
                     pixel |= (255 << a_off);
                }
                
                *((uint32_t*)(fbp + location)) = pixel;

            } else if (vinfo.bits_per_pixel == 24) {
                 // Fallback to byte writing if offsets align to bytes
                 if (r_off % 8 == 0 && g_off % 8 == 0 && b_off % 8 == 0) {
                     fbp[location + (r_off / 8)] = 255;
                     fbp[location + (g_off / 8)] = 255;
                     fbp[location + (b_off / 8)] = 0;
                 }
            } else if (vinfo.bits_per_pixel == 16) {
                // RGB565 usually: Red 5 bits, Green 6 bits, Blue 5 bits
                uint16_t pixel = 0;
                // Scale 255 to the bit length (simple shift approximation)
                int r = 255 >> (8 - vinfo.red.length);
                int g = 255 >> (8 - vinfo.green.length);
                int b = 0 >> (8 - vinfo.blue.length);
                
                pixel |= (r << r_off);
                pixel |= (g << g_off);
                pixel |= (b << b_off);
                
                *((uint16_t*)(fbp + location)) = pixel;
            }
        }
    }

    printf("Screen painted yellow!\n");

    munmap(fbp, screensize);
    close(fb_fd);
    return 0;
}
