#include "protocol.h"
#include "crc16-ccitt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

static const uint8_t SIGNATURE[P2P_SIGNATURE_LENGTH] = {
    'p', '2', 'p', 'm', 'u'
};

int p2p_validate_header(const struct p2p_header *header) {
    size_t i;
    uint16_t crc_header;

    if (!header) return false;

    for (i = 0; i < P2P_SIGNATURE_LENGTH; ++i)
        if (SIGNATURE[i] != header->signature[i]) return p2p_header_invalid_signature;

    if (header->cmd >= P2P_CMD_MAX) return p2p_header_invalid_cmd;

    crc_header = crc16_ccitt((uint8_t *)(header),
                             sizeof(struct p2p_header)
                              - sizeof(struct p2p_header::crc_header)
                              - sizeof(struct p2p_header::crc_data));

    if (crc_header != header->crc_header) return p2p_header_invalid_crc_header;

    switch (header->cmd) {
        case P2P_CMD_CONNECT:
            if (header->length != sizeof(struct p2p_connect_request))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_REFERENCE:
            if (header->length < sizeof(struct p2p_reference))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_CHANNEL_SWITCH:
            if (header->length != sizeof(struct p2p_channel_switch))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_REFERENCE_ADD:
            if (header->length != sizeof(struct p2p_reference_add))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_REFERENCE_REMOVE:
            if (header->length != sizeof(struct p2p_reference_remove))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_QUIT:
            if (header->length != sizeof(struct p2p_quit))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_PING:
            if (header->length != sizeof(struct p2p_ping))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_ACCEPT:
            if (header->length != sizeof(struct p2p_accept))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_MESSAGE:
            break;
    }

    return p2p_header_valid;
}

uint16_t p2p_utilize_packet(const struct p2p_header *header,
                            void **handlers, void *ctx) {
    typedef uint16_t (*unified_handler_t)(const void *data, void *ctx);
    uint16_t ret = 0;
    unified_handler_t handler;
    uint8_t *data;
    uint16_t data_crc;

    ret = p2p_validate_header(header);
    if (ret) return (ret | p2p_pvc_header_invalid);

    data = (const uint8_t *)(header + 1);
    data_crc = crc16_ccitt((uint8_t *)(header),
                           sizeof(struct p2p_header)
                            - sizeof(struct p2p_header::crc_header)
                            - sizeof(struct p2p_header::crc_data));
    if (data_crc != header->crc_data) return p2p_pvc_data_invalid_crc;

    handler = (unified_handler_t)handlers[header->cmd];
    if (!handler) return p2p_pvc_no_handler;

    ret = handler(data, ctx);
    if (ret) return ret | p2p_pvc_handler_error;

    return p2p_pvc_ok;
}

struct p2p_header *p2p_skip_packet(const struct p2p_header *header) {
    return (struct p2p_header *)(((uint8_t *)(header + 1)) + header->length);
}
