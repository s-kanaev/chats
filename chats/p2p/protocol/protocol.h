#ifndef _P2P_MU_DEFS_H_
# define _P2P_MU_DEFS_H_

# include <stdbool.h>
# include <stdint.h>

# define P2P_PACKED __attribute__((packed))

# define P2P_SIGNATURE_LENGTH 5
# define P2P_NICKNAME_LENGTH 16

typedef enum command_enum {
    P2P_CMD_CONNECT             = 0x00,
    P2P_CMD_REFERENCE           = 0x01,
    P2P_CMD_CHANNEL_SWITCH      = 0x02,
    P2P_CMD_REFERENCE_ADD       = 0x03,
    P2P_CMD_REFERENCE_REMOVE    = 0x04,
    P2P_CMD_QUIT                = 0x05,
    P2P_CMD_PING                = 0x06,
    P2P_CMD_ACCEPT              = 0x07,
    P2P_CMD_MESSAGE             = 0x08,
    P2P_CMD_MAX
} command_t;

struct p2p_header {
    uint8_t signature[P2P_SIGNATURE_LENGTH];
    uint8_t cmd;
    uint16_t length;
    uint16_t crc_header;
    uint16_t crc_data;
} P2P_PACKED;

struct p2p_connect_request {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;

struct p2p_reference {
    uint16_t clients_count;
} P2P_PACKED;

struct p2p_reference_entry {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
    uint16_t port;
    uint8_t ip_first_char;
} P2P_PACKED;

struct p2p_channel_switch {
} P2P_PACKED;

struct p2p_reference_add {
    struct p2p_reference_entry entry;
} P2P_PACKED;

struct p2p_reference_remove {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;

struct p2p_quit {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;

struct p2p_ping {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;

struct p2p_accept {
    uint8_t code;
} P2P_PACKED;

struct p2p_message {
    uint8_t msg_first_char;
} P2P_PACKED;

typedef enum header_validation_result_enum {
    header_valid,
    header_invalid_signature,
    header_invalid_cmd,
    header_invalid_length,
    header_invalid_crc_header
} header_validation_result_t;

int p2p_validate_header(const struct p2p_header *header);

#endif /* _P2P_MU_DEFS_H_ */