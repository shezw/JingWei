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
#include "protocol.h"

int main() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, JW_MT_SOCKET_PATH);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(1);
    }

    uint8_t buffer[1024]; 
    int rlen = 0;
    
    // Helper: Request Buffer for sending
    uint8_t req_buf[256];
    jw_msg_header_t *hdr = (jw_msg_header_t*)req_buf;
    
    // 1. Create Display
    printf("Sending Create Display Client2...\n");
    hdr->type = JW_MSG_TYPE_CMD;
    hdr->cmd = JW_CMD_CREATE_DISPLAY;
    hdr->msg_id = 1;
    hdr->len = sizeof(jw_msg_header_t) + sizeof(jw_payload_create_display_t);
    
    jw_payload_create_display_t *p_disp = (jw_payload_create_display_t*)(req_buf + sizeof(jw_msg_header_t));
    strcpy(p_disp->name, "Client2");
    p_disp->w = 640;
    p_disp->h = 480;
    
    req_buf[6] = jw_calculate_checksum(req_buf, hdr->len);
    send(sock, req_buf, hdr->len, 0);
    
    // Wait for response
    rlen = recv(sock, buffer, 1024, 0);
    if (rlen < sizeof(jw_msg_header_t)) { fprintf(stderr, "Invalid response len\n"); exit(1); }
    
    if (!jw_validate_checksum((uint8_t*)buffer, rlen)) { fprintf(stderr, "Invalid response Checksum\n"); exit(1); }

    jw_msg_header_t *resp_hdr = (jw_msg_header_t*)buffer;
    if (resp_hdr->type != JW_MSG_TYPE_RESP) { fprintf(stderr, "Not a response type\n"); exit(1); }
    
    jw_payload_response_t *resp_data = (jw_payload_response_t*)(buffer + sizeof(jw_msg_header_t));
    if (resp_data->status != 0) { fprintf(stderr, "Create Display Failed\n"); exit(1); }
    
    int display_id = resp_data->data.new_id;
    printf("Client 2: Display created, ID: %d\n", display_id);

    // 2. Create Canvas
    printf("Sending Create Canvas...\n");
    hdr->cmd = JW_CMD_CREATE_CANVAS;
    hdr->msg_id = 2;
    hdr->len = sizeof(jw_msg_header_t) + sizeof(jw_payload_create_canvas_t);
    
    jw_payload_create_canvas_t *p_cvs = (jw_payload_create_canvas_t*)(req_buf + sizeof(jw_msg_header_t));
    p_cvs->display_id = display_id;
    p_cvs->w = 640;
    p_cvs->h = 480;
    
    req_buf[6] = jw_calculate_checksum(req_buf, hdr->len);
    send(sock, req_buf, hdr->len, 0);
    
    rlen = recv(sock, buffer, 1024, 0);
    if (rlen < sizeof(jw_msg_header_t)) { fprintf(stderr, "Invalid response len\n"); exit(1); }
    resp_hdr = (jw_msg_header_t*)buffer; 
    resp_data = (jw_payload_response_t*)(buffer + sizeof(jw_msg_header_t));
    
    // Note: status 0 is OK.
    if (resp_data->status != 0) { fprintf(stderr, "Create Canvas Failed: %s\n", resp_data->data.message); exit(1); }
    
    char shm_name[64];
    strncpy(shm_name, resp_data->data.message, 63);
    printf("Shared Memory: %s\n", shm_name);

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(1); }
    
    uint32_t *canvas = mmap(0, 640 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (canvas == MAP_FAILED) { perror("mmap"); exit(1); }

    // Render Loop
    printf("Starting render loop...\n");
    int r = 0, g = 0, b = 0;
    while(1) {
        for(int i=0; i < 640*480; i++) {
            canvas[i] = (255 << 24) | (r << 16) | (g << 8) | b;
        }

        r = (r + 8) % 255;
        g = (g + 4) % 255;
        b = (b + 2) % 255;

        // Commit (Binary)
        hdr->cmd = JW_CMD_COMMIT;
        hdr->msg_id++;
        hdr->len = sizeof(jw_msg_header_t) + sizeof(jw_payload_commit_t);
        
        jw_payload_commit_t *p_commit = (jw_payload_commit_t*)(req_buf + sizeof(jw_msg_header_t));
        p_commit->display_id = display_id;
        
        req_buf[6] = jw_calculate_checksum(req_buf, hdr->len);
        send(sock, req_buf, hdr->len, 0);
        
        rlen = recv(sock, buffer, 1024, 0); // Wait for ACK
        // Should validate ACK but keeping simple loop speed

        usleep(33000); // 30 FPS = ~33ms
    }

    close(sock);
    return 0;
}
