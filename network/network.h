#ifndef _CHATS_NETWORK_COMMON_H_
# define _CHATS_NETWORK_COMMON_H_

# include "endpoint.h"
# include "memory.h"
# include "io-service.h"
# include <stddef.h>
# include <sys/types.h>

/** callback on connection accept
 * \param [in] ep connected remote endpoint
 * \param [in] ctx user context
 * \param [in] err errno
 * \return \c true until the connection is rejected
 */
typedef bool (*tcp_connection_cb_t)(endpoint_t *ep, int err,
                                    void *ctx);
/** callback on data sent
 * \param [in] err errno
 * \param [in] bytes bytes sent
 * \param [in] buffer buffer sent
 * \param [in] ctx user context
 */
typedef void (*network_send_recv_cb_t)(int err,
                                       size_t bytes, buffer_t *buffer,
                                       void *ctx);

typedef enum network_operation_enum {
    NETWORK_OP_RECEIVE = 0,
    NETWORK_OP_SEND = 1,
    NETWORK_OP_COUNT
} network_op_t;

struct connection_acceptor {
    void *host;
    tcp_connection_cb_t connection_cb;
    void *connection_ctx;
};

struct send_recv_buffer {
    network_op_t op;
    void *host;
    buffer_t *buffer;
    size_t bytes_operated;
    network_send_recv_cb_t cb;
    void *ctx;
};

typedef ssize_t (*NETWORK_OPERATOR)(int skt_fd, void *b, size_t len, int flags);

struct network_operation {
    NETWORK_OPERATOR op;
    io_svc_op_t io_svc_op;
};

const struct network_operation OPERATORS[NETWORK_OP_COUNT];

#endif /* _CHATS_NETWORK_COMMON_H_ */