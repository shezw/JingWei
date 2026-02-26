#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>

#define SOCKET_PATH "/tmp/jw_mt_core.sock"

int main() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(1);
    }

    char buffer[1024];
    
    // Create Display
    sprintf(buffer, "create_display Client1 800 480");
    send(sock, buffer, strlen(buffer), 0);
    int rlen = recv(sock, buffer, 1024, 0); 
    if (rlen > 0) buffer[rlen] = '\0';
    int display_id = atoi(buffer);
    printf("Client 1: Display created, ID: %d\n", display_id);

    // Create Canvas
    sprintf(buffer, "create_canvas %d 800 480", display_id);
    send(sock, buffer, strlen(buffer), 0);
    int len = recv(sock, buffer, 1024, 0); 
    if (len > 0) buffer[len] = '\0';
    char shm_name[64];
    strcpy(shm_name, buffer);

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(1); }
    
    uint32_t *canvas = mmap(0, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Render Loop
    int r = 0, g = 0, b = 0;
    while(1) {
        for(int i=0; i < 800*480; i++) {
            canvas[i] = (255 << 24) | (r << 16) | (g << 8) | b;
        }

        r = (r + 2) % 255;
        g = (g + 5) % 255;
        b = (b + 8) % 255;

        sprintf(buffer, "commit %d", display_id);
        send(sock, buffer, strlen(buffer), 0);
        recv(sock, buffer, 1024, 0); // Wait for ACK

        usleep(100000); // 10 FPS = 100ms
    }

    close(sock);
    return 0;
}
