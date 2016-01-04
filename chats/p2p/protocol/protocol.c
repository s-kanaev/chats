#include "protocol.h"
#include "crc16-ccitt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define member_size(type, member) sizeof(((type *)0)->member)

const uint8_t P2P_SIGNATURE[P2P_SIGNATURE_LENGTH] = {
    'p', '2', 'p', 'm', 'u'
};

uint16_t p2p_validate_header(const p2p_header_t *header) {
    size_t i;
    uint16_t crc_header;

    if (!header) return false;

    for (i = 0; i < P2P_SIGNATURE_LENGTH; ++i)
        if (P2P_SIGNATURE[i] != header->signature[i]) return p2p_header_invalid_signature;

    if (header->cmd >= P2P_CMD_MAX) return p2p_header_invalid_cmd;

    crc_header = crc16_ccitt((uint8_t *)(header),
                             sizeof(p2p_header_t)
                              - member_size(p2p_header_t, crc_header)
                              - member_size(p2p_header_t, crc_data));

    if (crc_header != header->crc_header) return p2p_header_invalid_crc_header;

    switch (header->cmd) {
        case P2P_CMD_CONNECT:
            if (header->length != sizeof(p2p_connect_request_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_REFERENCE:
            if (header->length < sizeof(p2p_reference_t))
                return p2p_header_invalid_length;
            if ((header->length - sizeof(p2p_reference_t)) % sizeof(p2p_reference_entry_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_CHANNEL_SWITCH:
            if (header->length != sizeof(p2p_channel_switch_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_REFERENCE_ADD:
            if (header->length != sizeof(p2p_reference_add_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_REFERENCE_REMOVE:
            if (header->length != sizeof(p2p_reference_remove_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_QUIT:
            if (header->length != sizeof(p2p_quit_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_PING:
            if (header->length != sizeof(p2p_ping_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_ACCEPT:
            if (header->length != sizeof(p2p_accept_t))
                return p2p_header_invalid_length;
            break;
        case P2P_CMD_MESSAGE:
            break;
    }

    return p2p_header_valid;
}

uint16_t p2p_check_data_crc(const p2p_header_t *header) {
    const uint8_t *data;
    uint16_t data_crc;

    data = (const uint8_t *)(header + 1);
    data_crc = crc16_ccitt(data,
                           header->length);
    if (data_crc != header->crc_data) return p2p_pvc_data_invalid_crc;

    return 0;
}

uint16_t p2p_utilize_packet(const p2p_header_t *header,
                            void **handlers, void *ctx) {
    typedef uint16_t (*unified_handler_t)(const void *data, void *ctx);
    uint16_t ret = 0;
    unified_handler_t handler;
    const uint8_t *data;
    uint16_t data_crc;

    ret = p2p_validate_header(header);
    if (ret) return (ret | p2p_pvc_header_invalid);

    ret = p2p_check_data_crc(header);
    if (ret) return ret;

    handler = (unified_handler_t)handlers[header->cmd];
    if (!handler) return p2p_pvc_no_handler;

    ret = handler(data, ctx);
    if (ret) return ret | p2p_pvc_handler_error;

    return p2p_pvc_ok;
}

p2p_header_t *p2p_skip_packet(const p2p_header_t *header) {
    return (struct p2p_header *)(((uint8_t *)(header + 1)) + header->length);
}

void p2p_put_header_crc(p2p_header_t *header) {
    uint16_t crc_header;

    if (!header) return;
    crc_header = crc16_ccitt((uint8_t *)(header),
                             sizeof(p2p_header_t)
                              - member_size(p2p_header_t, crc_header)
                              - member_size(p2p_header_t, crc_data));
    header->crc_header = crc_header;
}

void p2p_put_data_crc(p2p_header_t *header) {
    const uint8_t *data;
    uint16_t data_crc;

    if (!header) return;

    data = (const uint8_t *)(header + 1);
    data_crc = crc16_ccitt(data,
                           header->length);
    header->crc_data = data_crc;
}

