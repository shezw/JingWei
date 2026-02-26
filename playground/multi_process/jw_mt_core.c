/**
    -----------------------------------------------------------

 	Project JingWei
 	playground mt_core.c    2026/02/26
 	
 	@link    : https://github.com/shezw/jingwei
 	@author	 : shezw
 	@email	 : hello@shezw.com

    -----------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <SDL2/SDL.h>

#define SOCKET_PATH "/tmp/jw_mt_core.sock"
#define MAX_CLIENTS 10

// Simple structure to manage displays (SDL Windows)
typedef struct jw_display {
    int id;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int w, h;
    char shm_name[64];
    int shm_fd;
    void *shm_ptr;
    struct jw_display *next;
} jw_display_t;

jw_display_t *g_displays = NULL;
int g_display_id_counter = 0;

// Find display by ID
jw_display_t* find_display(int id) {
    jw_display_t *curr = g_displays;
    while(curr) {
        if(curr->id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // SDL Init
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Socket Setup
    int server_fd;
    struct sockaddr_un addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        return 1;
    }

    // Client management
    int client_sockets[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    printf("jw_mt_core started on %s...\n", SOCKET_PATH);

    bool running = true;
    while(running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0 ; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        // Use timeout for select so we can poll SDL events
        struct timeval tv = {0, 10000}; // 10ms
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        if (activity > 0) {
            // Incoming connection
            if (FD_ISSET(server_fd, &readfds)) {
                int new_socket = accept(server_fd, NULL, NULL);
                if (new_socket < 0) {
                    perror("accept");
                } else {
                    printf("New connection, socket fd is %d\n", new_socket);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (client_sockets[i] == 0) {
                            client_sockets[i] = new_socket;
                            break;
                        }
                    }
                }
            }

            // IO operations on some other socket
            for (int i = 0; i < MAX_CLIENTS; i++) {
                int sd = client_sockets[i];
                if (sd > 0 && FD_ISSET(sd, &readfds)) {
                    char buffer[1024] = {0};
                    ssize_t valread = read(sd, buffer, 1024);
                    
                    if (valread <= 0) {
                        // Somebody disconnected
                        getpeername(sd, (struct sockaddr*)&addr, (socklen_t*)&addr);
                        printf("Host disconnected, fd %d\n", sd);
                        close(sd);
                        client_sockets[i] = 0;
                    } else {
                        // Protocol: Simple text based for this experiment
                        // CMD [ARGS...]
                        
                        char cmd[32] = {0};
                        sscanf(buffer, "%s", cmd);

                        if(strcmp(cmd, "create_display") == 0) {
                            char name[64] = {0};
                            int w = 0, h = 0;
                            sscanf(buffer, "create_display %s %d %d", name, &w, &h);
                            
                            printf("Creating display: %s (%dx%d)\n", name, w, h);

                            jw_display_t *new_disp = (jw_display_t*)malloc(sizeof(jw_display_t));
                            memset(new_disp, 0, sizeof(jw_display_t));
                            new_disp->id = ++g_display_id_counter;
                            new_disp->w = w;
                            new_disp->h = h;
                            new_disp->window = SDL_CreateWindow(name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
                            
                            if (new_disp->window) {
                                new_disp->renderer = SDL_CreateRenderer(new_disp->window, -1, SDL_RENDERER_ACCELERATED);
                                if (new_disp->renderer) {
                                     new_disp->texture = SDL_CreateTexture(new_disp->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
                                }
                            }

                            if (!new_disp->texture) {
                                printf("Failed to create SDL resources: %s\n", SDL_GetError());
                            } else {
                                new_disp->next = g_displays;
                                g_displays = new_disp;
                            }
                            
                            char resp[32];
                            snprintf(resp, sizeof(resp), "%d", new_disp->id);
                            send(sd, resp, strlen(resp), 0);

                        } else if (strcmp(cmd, "create_canvas") == 0) {
                             int did = 0;
                             int w = 0, h = 0; 
                             sscanf(buffer, "create_canvas %d %d %d", &did, &w, &h);
                             
                             jw_display_t *disp = find_display(did);
                             char resp[128] = "ERROR";
                             
                             if (disp) {
                                 snprintf(disp->shm_name, sizeof(disp->shm_name), "/jw_shm_%d", did);
                                 
                                 // Unlink if exists, ensures clean start
                                 shm_unlink(disp->shm_name);

                                 // On macOS, shm_open requires the name to start with /
                                 // Also, permission bits 0666 might be modified by umask
                                 int fd = shm_open(disp->shm_name, O_CREAT | O_RDWR, 0666);
                                 if (fd >= 0) {
                                     // Ensure size is big enough
                                     if (ftruncate(fd, disp->w * disp->h * 4) == -1) {
                                          perror("ftruncate failed");
                                          close(fd);
                                          shm_unlink(disp->shm_name);
                                     } else {
                                         disp->shm_fd = fd;
                                         disp->shm_ptr = mmap(0, disp->w * disp->h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                                         
                                         if (disp->shm_ptr != MAP_FAILED) {
                                             snprintf(resp, sizeof(resp), "%s", disp->shm_name);
                                         } else {
                                             perror("mmap failed");
                                             close(fd);
                                             shm_unlink(disp->shm_name);
                                         }
                                     }
                                 } else {
                                     // If we fail here, print exact error
                                     fprintf(stderr, "shm_open failed for %s: %s\n", disp->shm_name, strerror(errno));
                                 }
                             } else {
                                 printf("WARN: Display ID %d not found\n", did);
                             }
                             send(sd, resp, strlen(resp), 0);
                             
                        } else if (strcmp(cmd, "commit") == 0) {
                             int did = 0;
                             sscanf(buffer, "commit %d", &did);
                             jw_display_t *disp = find_display(did);
                             
                             if (disp && disp->shm_ptr && disp->texture) {
                                 // Copy pixels from SHM to Texture
                                 void *pixels;
                                 int pitch;
                                 if (SDL_LockTexture(disp->texture, NULL, &pixels, &pitch) == 0) {
                                     // SDL Texture Pitch might differ from pure width * 4 if memory aligned
                                     // But for simplicity if we match 32bit ARGB, it's usually width * 4
                                     memcpy(pixels, disp->shm_ptr, disp->w * disp->h * 4);
                                     SDL_UnlockTexture(disp->texture);
                                 }
                                 
                                 SDL_RenderClear(disp->renderer);
                                 SDL_RenderCopy(disp->renderer, disp->texture, NULL, NULL);
                                 SDL_RenderPresent(disp->renderer);
                             }
                             // Simple ACK
                             send(sd, "OK", 2, 0);
                        }
                    }
                }
            }
        }
        
        // Handle SDL Events
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
        }
    }
    
    // Cleanup
    close(server_fd);
    unlink(SOCKET_PATH);
    SDL_Quit();
    return 0;
}
