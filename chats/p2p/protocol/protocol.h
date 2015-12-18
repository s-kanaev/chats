#ifndef _P2P_MU_PROTOCOL_H_
# define _P2P_MU_PROTOCOL_H_

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
typedef struct p2p_header p2p_header_t;

struct p2p_connect_request {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;
typedef struct p2p_connect_request p2p_connect_request_t;

struct p2p_reference {
    uint16_t clients_count;
} P2P_PACKED;
typedef struct p2p_reference p2p_reference_t;

struct p2p_reference_entry {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
    uint16_t port;
    uint8_t ip_first_char;
} P2P_PACKED;
typedef struct p2p_reference_entry p2p_reference_entry_t;

struct p2p_channel_switch {
} P2P_PACKED;
typedef struct p2p_channel_switch p2p_channel_switch_t;

struct p2p_reference_add {
    struct p2p_reference_entry entry;
} P2P_PACKED;
typedef struct p2p_reference_add p2p_reference_add_t;

struct p2p_reference_remove {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;
typedef struct p2p_reference_remove p2p_reference_remove_t;

struct p2p_quit {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;
typedef struct p2p_quit p2p_quit_t;

struct p2p_ping {
    uint8_t nickname[P2P_NICKNAME_LENGTH];
} P2P_PACKED;
typedef struct p2p_ping p2p_ping_t;

struct p2p_accept {
    uint8_t code;
} P2P_PACKED;
typedef struct p2p_accept p2p_accept_t;

struct p2p_message {
    uint8_t msg_first_char;
} P2P_PACKED;
typedef struct p2p_message p2p_message_t;

# define PACKET_VALIDATION_CODE(x)      ((x) & 0xff00)
typedef enum p2p_packet_validation_code_enum {
    p2p_pvc_ok                      = 0x0000,
    p2p_pvc_header_invalid          = 0x0100,
    p2p_pvc_data_invalid_crc        = 0x0200,
    p2p_pvc_no_handler              = 0x0400,
    p2p_pvc_handler_error           = 0x0800
} p2p_packet_validation_code_t;

typedef enum p2p_header_validation_result_enum {
    p2p_header_valid                = 0x0000,
    p2p_header_invalid_signature    = 0x0001,
    p2p_header_invalid_cmd          = 0x0002,
    p2p_header_invalid_length       = 0x0003,
    p2p_header_invalid_crc_header   = 0x0004
} p2p_header_validation_result_t;

/** Validate \c p2p_header
 * \param header
 * \return \c header_validation_result_t
 */
uint16_t p2p_validate_header(const p2p_header_t *header);

typedef enum p2p_connect_code_enum {
    p2p_connect_ok              = 0x0000,
    p2p_connect_duplicate       = 0x0001
} p2p_connect_code_t;

typedef enum p2p_reference_code_enum {
    p2p_reference_ok            = 0x0001,
    p2p_reference_failed        = 0x0002
} p2p_reference_code_t;

typedef enum p2p_ref_add_code_enum {
    p2p_ref_add_ok              = 0x0000,
    p2p_ref_add_duplicate_name  = 0x0001,
    p2p_ref_add_duplicate_addr  = 0x0002
} p2p_ref_add_code_t;

typedef enum p2p_ref_del_code_enum {
    p2p_ref_del_ok              = 0x0000,
    p2p_ref_del_not_found       = 0x0001
} p2p_ref_del_code_t;

typedef enum p2p_channel_switch_code_enum {
    p2p_channel_switch_success  = 0x0000,
    p2p_channel_switch_fail     = 0x0001
} p2p_channel_switch_code_t;

typedef enum p2p_quit_code_enum {
    p2p_quit_success            = 0x0000,
    p2p_quit_not_found          = 0x0001,
    p2p_quit_failed             = 0x0002
} p2p_quit_code_t;

typedef enum p2p_ping_code_enum {
    p2p_ping_ok                 = 0x0000,
    p2p_ping_not_found          = 0x0001,
    p2p_ping_failed             = 0x0002
} p2p_ping_code_t;

typedef enum p2p_accept_code_enum {
    p2p_accept_ok               = 0x0000,
    p2p_accept_failed           = 0x0001
} p2p_accept_code_t;

typedef enum p2p_message_code_enum {
    p2p_message_ok              = 0x0000,
    p2p_message_failed          = 0x0001
} p2p_message_code_t;

/** Handle connection request
 * \param cr connection request
 * \param ctx context
 * \return p2p_connect_code_t
 */
typedef uint16_t
        (*p2p_connect_handler)(const p2p_connect_request_t *cr,
                               void *ctx);
/** Handle reference
 * \param r reference
 * \param ctx context
 * \return p2p_ref_add_code_t
 */
typedef uint16_t
        (*p2p_reference_handler)(const p2p_reference_t *r,
                                 void *ctx);
/** Handle reference addition
 * \param re reference entry
 * \param ctx context
 * \return p2p_ref_add_code_t
 */
typedef uint16_t
        (*p2p_reference_add_handler)(const p2p_reference_entry_t *re,
                                     void *ctx);
/** Handle reference removal
 * \param rr reference to remove
 * \param ctx context
 * \return p2p_ref_del_code_t
 */
typedef uint16_t
        (*p2p_reference_del_handler)(const p2p_reference_remove_t *rr,
                                     void *ctx);
/** Handle channel switch request
 * \param cs channel switch request
 * \param ctx context
 * \return p2p_channel_switch_code_t
 */
typedef uint16_t
        (*p2p_channel_switch_handler)(const p2p_channel_switch_t *cs,
                                      void *ctx);
/** Handle quit request
 * \param q quit request
 * \param ctx context
 * \return p2p_quit_code_t
 */
typedef uint16_t
        (*p2p_quit_handler)(const p2p_quit_t *q,
                            void *ctx);
/** Handle ping request
 * \param ping ping request
 * \param ctx context
 * \return p2p_ping_code_t
 */
typedef uint16_t
        (*p2p_ping_handler)(const p2p_ping_t *p,
                            void *ctx);
/** Handle acception code
 * \param a acception code
 * \param ctx context
 * \return p2p_accept_code_t
 */
typedef uint16_t
        (*p2p_accept_handler)(const p2p_accept_t *a,
                              void *ctx);
/** Handle message
 * \param m message
 * \param ctx context
 * \return p2p_message_code_t
 */
typedef uint16_t
        (*p2p_message_handler)(const p2p_message_t *m,
                               void *ctx);

/** Utilize packet with \c header as header
 * \param header
 * \param handlers handlers of different packets
 * \return bitwise OR'ed mask of \c packet_validation_code_t and ... TODO ...
 */
uint16_t p2p_utilize_packet(const p2p_header_t *header,
                            void **handlers, void *ctx);

/** Skip this packet
 * \param header current packet header
 * \return pointer to next packet header
 *
 * Assuming \c header is valid.
 */
p2p_header_t *p2p_skip_packet(const p2p_header_t *header);

#endif /* _P2P_MU_PROTOCOL_H_ */
