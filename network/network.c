#include "network.h"
#include "io-service.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

const struct network_operation OPERATORS[NETWORK_OP_COUNT] = {
    [NETWORK_OP_RECEIVE] = {
        .op = recv,
        .io_svc_op = IO_SVC_OP_READ
    },
    [NETWORK_OP_SEND] = {
        .op = send,
        .io_svc_op = IO_SVC_OP_WRITE
    }
};
