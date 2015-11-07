#include "protocol.h"
#include <stdbool.h>
#include <stdint.h>

static const uint8_t SIGNATURE[P2P_SIGNATURE_LENGTH] = {
    'p', '2', 'p', 'm', 'u'
};

bool p2p_validate_header(p2p_header *header) {
    size_t i;

    if (!header) return false;

    for (i = 0; i < P2P_SIGNATURE_LENGTH; ++i)
        if (SIGNATURE[i] != header->signature[i]) return false;

    return true;
}
