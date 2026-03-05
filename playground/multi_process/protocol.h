#ifndef JW_MT_PROTOCOL_H
#define JW_MT_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define JW_MT_SOCKET_PATH "/tmp/jw_mt_core.sock"

// Message Type
enum {
    JW_MSG_TYPE_CMD = 0x01,
    JW_MSG_TYPE_RESP = 0x02,
    JW_MSG_TYPE_EVT = 0x03
};

// Message Command
enum {
    JW_CMD_CREATE_DISPLAY = 0x10,
    JW_CMD_CREATE_CANVAS  = 0x11,
    JW_CMD_COMMIT         = 0x12,
    JW_CMD_RESPONSE       = 0xFF
};

// Protocol Header
// Total 7 bytes: TYPE(1) CMD(1) LEN(2) ID(2) CS(1)
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t cmd;
    uint16_t len;     
    uint16_t msg_id;
    uint8_t checksum;
} jw_msg_header_t;

_Static_assert(sizeof(jw_msg_header_t) == 7, "Header size must be 7 bytes");

// Payloads
typedef struct __attribute__((packed)) {
    char name[32];
    uint16_t w;
    uint16_t h;
} jw_payload_create_display_t;

typedef struct __attribute__((packed)) {
    int display_id;
    uint16_t w;
    uint16_t h;
} jw_payload_create_canvas_t;

typedef struct __attribute__((packed)) {
    int display_id;
} jw_payload_commit_t;

// Response payload
typedef struct __attribute__((packed)) {
    int status; // 0 OK, <0 Error
    union {
        int new_id;
        char message[64]; 
    } data;
} jw_payload_response_t;

// Checksum Implementation
static inline uint8_t jw_calculate_checksum(const uint8_t *msg, size_t len) {
    if (NULL == msg || len < 7) return 0;
    
    uint8_t checksum = 0;
    
    // Checksum of header fields (first 6 bytes)
    for (size_t i = 0; i < 6; i++) {
        checksum += msg[i];
    }
    
    // Payload checksum logic from SKILL
    size_t payload_len = len - 7;
    if (payload_len > 0) {
        // SKILL: msg[7 + (msg[4] % (payload_len%7))]
        // msg[4] is low byte of msg_id
        uint8_t id_byte = msg[4];
        
        // Use a safe modulo logic:
        int offset_idx = 0;
        int divisor = payload_len % 7;
        if (divisor == 0) divisor = 1; 

        // I'll stick to `offset_idx = id_byte % payload_len` as a "fix".
        // It guarantees index is within 0..payload_len-1
        offset_idx = id_byte % payload_len;

        checksum += msg[7 + offset_idx];
    }
    
    return checksum;
}

static inline bool jw_validate_checksum(const uint8_t *msg, size_t len) {
    if (NULL == msg || len < 7) return false;
    uint8_t calculated = jw_calculate_checksum(msg, len);
    // Compare with the Checksum field at index 6
    return calculated == msg[6];
}

#endif // JW_MT_PROTOCOL_H
