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
#include "protocol.h"

// #define JW_MT_SOCKET_PATH "/tmp/jw_mt_core.sock" // Moved to protocol.h 
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
    strncpy(addr.sun_path, JW_MT_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(JW_MT_SOCKET_PATH);

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

    printf("jw_mt_core started on %s...\n", JW_MT_SOCKET_PATH);

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
                        // Protocol: Binary
                        if (valread < sizeof(jw_msg_header_t)) {
                             fprintf(stderr, "Received incomplete header (%ld bytes)\n", valread);
                             continue;
                        }
                        
                        jw_msg_header_t *hdr = (jw_msg_header_t*)buffer;
                        
                        // Validate Checksum (simple full buffer check)
                        if (!jw_validate_checksum((uint8_t*)buffer, valread)) {
                             fprintf(stderr, "Checksum mismatch! Expected %d\n", hdr->checksum);
                             continue;
                        }
                        
                        // Process Command
                        if (hdr->type == JW_MSG_TYPE_CMD) {
                            switch (hdr->cmd) {
                                case JW_CMD_CREATE_DISPLAY: {
                                    if (valread < sizeof(jw_msg_header_t) + sizeof(jw_payload_create_display_t)) break;
                                    jw_payload_create_display_t *p = (jw_payload_create_display_t*)(buffer + sizeof(jw_msg_header_t));
                                    
                                    printf("CMD: Create Display '%s' (%dx%d)\n", p->name, p->w, p->h);
                                    
                                    jw_display_t *new_disp = (jw_display_t*)malloc(sizeof(jw_display_t));
                                    memset(new_disp, 0, sizeof(jw_display_t));
                                    new_disp->id = ++g_display_id_counter;
                                    new_disp->w = p->w;
                                    new_disp->h = p->h;
                                    new_disp->window = SDL_CreateWindow(p->name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, p->w, p->h, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
                                    
                                    if (new_disp->window) {
                                        new_disp->renderer = SDL_CreateRenderer(new_disp->window, -1, SDL_RENDERER_ACCELERATED);
                                        if (new_disp->renderer) {
                                             new_disp->texture = SDL_CreateTexture(new_disp->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, p->w, p->h);
                                        }
                                    }

                                    if (!new_disp->texture) {
                                        printf("Failed to create SDL resources: %s\n", SDL_GetError());
                                    } else {
                                        new_disp->next = g_displays;
                                        g_displays = new_disp;
                                    }
                                    
                                    // Send Response
                                    jw_msg_header_t resp_hdr = {0};
                                    resp_hdr.type = JW_MSG_TYPE_RESP;
                                    resp_hdr.cmd = JW_CMD_RESPONSE;
                                    resp_hdr.msg_id = hdr->msg_id;
                                    resp_hdr.len = sizeof(jw_msg_header_t) + sizeof(jw_payload_response_t);
                                    
                                    jw_payload_response_t resp_data = {0};
                                    resp_data.status = new_disp->texture ? 0 : -1;
                                    resp_data.data.new_id = new_disp->id;
                                    
                                    uint8_t resp_buf[256];
                                    memcpy(resp_buf, &resp_hdr, sizeof(jw_msg_header_t));
                                    memcpy(resp_buf + sizeof(jw_msg_header_t), &resp_data, sizeof(jw_payload_response_t));
                                    resp_buf[6] = jw_calculate_checksum(resp_buf, resp_hdr.len);
                                    
                                    send(sd, resp_buf, resp_hdr.len, 0);
                                    break;
                                }
                                case JW_CMD_CREATE_CANVAS: {
                                    if (valread < sizeof(jw_msg_header_t) + sizeof(jw_payload_create_canvas_t)) break;
                                    jw_payload_create_canvas_t *p = (jw_payload_create_canvas_t*)(buffer + sizeof(jw_msg_header_t));
                                    
                                    printf("CMD: Create Canvas for Display %d (%dx%d)\n", p->display_id, p->w, p->h);
                                    
                                    jw_display_t *disp = find_display(p->display_id);
                                    int status = -1;
                                    char msg_buf[64] = "ERROR";
                                    
                                    if (disp) {
                                         snprintf(disp->shm_name, sizeof(disp->shm_name), "/jw_shm_%d", p->display_id);
                                         shm_unlink(disp->shm_name);
                                         int fd = shm_open(disp->shm_name, O_CREAT | O_RDWR, 0666);
                                         if (fd >= 0) {
                                             if (ftruncate(fd, disp->w * disp->h * 4) != -1) {
                                                 disp->shm_fd = fd;
                                                 disp->shm_ptr = mmap(0, disp->w * disp->h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                                                 if (disp->shm_ptr != MAP_FAILED) {
                                                     status = 0;
                                                     strncpy(msg_buf, disp->shm_name, 63);
                                                 } else {
                                                     perror("mmap failed"); close(fd); shm_unlink(disp->shm_name);
                                                 }
                                             } else {
                                                  perror("ftruncate"); close(fd); shm_unlink(disp->shm_name);
                                             }
                                         } else {
                                             fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
                                         }
                                    }
                                    
                                    // Response
                                    jw_msg_header_t resp_hdr = {0};
                                    resp_hdr.type = JW_MSG_TYPE_RESP;
                                    resp_hdr.cmd = JW_CMD_RESPONSE;
                                    resp_hdr.msg_id = hdr->msg_id;
                                    resp_hdr.len = sizeof(jw_msg_header_t) + sizeof(jw_payload_response_t);
                                    
                                    jw_payload_response_t resp_data = {0};
                                    resp_data.status = status;
                                    strncpy(resp_data.data.message, msg_buf, 63);
                                    
                                    uint8_t resp_buf[256];
                                    memcpy(resp_buf, &resp_hdr, sizeof(jw_msg_header_t));
                                    memcpy(resp_buf + sizeof(jw_msg_header_t), &resp_data, sizeof(jw_payload_response_t));
                                    resp_buf[6] = jw_calculate_checksum(resp_buf, resp_hdr.len);
                                    send(sd, resp_buf, resp_hdr.len, 0);
                                    break;
                                }
                                case JW_CMD_COMMIT: {
                                    if (valread < sizeof(jw_msg_header_t) + sizeof(jw_payload_commit_t)) break;
                                    jw_payload_commit_t *p = (jw_payload_commit_t*)(buffer + sizeof(jw_msg_header_t));
                                    
                                    jw_display_t *disp = find_display(p->display_id);
                                    if (disp && disp->shm_ptr && disp->texture) {
                                        void *pixels;
                                        int pitch;
                                        if (SDL_LockTexture(disp->texture, NULL, &pixels, &pitch) == 0) {
                                            memcpy(pixels, disp->shm_ptr, disp->w * disp->h * 4);
                                            SDL_UnlockTexture(disp->texture);
                                        }
                                        SDL_RenderClear(disp->renderer);
                                        SDL_RenderCopy(disp->renderer, disp->texture, NULL, NULL);
                                        SDL_RenderPresent(disp->renderer);
                                    }
                                    
                                    // ACK
                                    jw_msg_header_t resp_hdr = {0};
                                    resp_hdr.type = JW_MSG_TYPE_RESP;
                                    resp_hdr.cmd = JW_CMD_RESPONSE;
                                    resp_hdr.msg_id = hdr->msg_id;
                                    resp_hdr.len = sizeof(jw_msg_header_t) + sizeof(jw_payload_response_t); // Fixed length response
                                    
                                    jw_payload_response_t resp_data = {0};
                                    resp_data.status = 0;
                                    
                                    uint8_t resp_buf[256];
                                    memcpy(resp_buf, &resp_hdr, sizeof(jw_msg_header_t));
                                    memcpy(resp_buf + sizeof(jw_msg_header_t), &resp_data, sizeof(jw_payload_response_t));
                                    resp_buf[6] = jw_calculate_checksum(resp_buf, resp_hdr.len);
                                    send(sd, resp_buf, resp_hdr.len, 0);
                                    break;
                                }
                                default:
                                    printf("Unknown CMD: %d\n", hdr->cmd);
                            }
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
    unlink(JW_MT_SOCKET_PATH);
    SDL_Quit();
    return 0;
}
