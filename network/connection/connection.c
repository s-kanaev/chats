#include "connection.h"
#include "network.h"
#include "io-service.h"

#include <assert.h>
#include <errno.h>

struct async_sr {
    struct send_recv_tcp_buffer *srb;
    connection_t *connection;
    io_service_t *iosvc;
};

static
void send_recv_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    struct async_sr *sr = ctx;
    struct send_recv_tcp_buffer *srb = sr->srb;
    connection_t *connection = sr->connection;
    io_service_t *iosvc = sr->iosvc;

    buffer_t *buffer = srb->buffer;
    size_t bytes_op = srb->bytes_operated;
    network_tcp_op_t op = srb->op;
    NETWORK_TCP_OPERATOR oper = TCP_OPERATORS[op].op;
    io_svc_op_t io_svc_op = TCP_OPERATORS[op].io_svc_op;
    ssize_t bytes_op_cur;

    errno = 0;
    bytes_op_cur = (*oper)(connection->ep_skt.skt,
                           buffer_data(buffer) + bytes_op,
                           buffer_size(buffer) - bytes_op,
                           MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytes_op_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(iosvc,
                                connection->ep_skt.skt,
                                io_svc_op,
                                true,
                                send_recv_async_tpl,
                                sr);
        else {
            (*srb->cb)(errno, bytes_op, buffer, srb->ctx);
            deallocate(srb);
            deallocate(sr);
        }
    }
    else {
        bytes_op += bytes_op_cur;
        srb->bytes_operated = bytes_op;
        if (bytes_op < buffer_size(buffer))
            io_service_post_job(iosvc,
                                connection->ep_skt.skt,
                                io_svc_op,
                                true,
                                send_recv_async_tpl,
                                sr);
        else {
            (*srb->cb)(errno, bytes_op, buffer, srb->ctx);
            deallocate(srb);
            deallocate(sr);
        }
    }
}

void connection_send_recv_sync(connection_t *connection,
                               struct send_recv_tcp_buffer *srb) {
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    network_tcp_op_t op;
    NETWORK_TCP_OPERATOR oper;

    assert(srb && connection &&
           connection->ep_skt.skt >= 0 &&
           connection->ep_skt.ep.ep_type == EPT_TCP);

    buffer = srb->buffer;
    op = srb->op;
    oper = TCP_OPERATORS[op].op;

    assert(buffer != NULL);

    bytes_op = srb->bytes_operated;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(connection->ep_skt.skt,
                               buffer_data(buffer) + bytes_op,
                               buffer_size(buffer) - bytes_op,
                               MSG_NOSIGNAL);
        if (bytes_op_cur < 0) break;

        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;

    if (srb->cb)
        (*srb->cb)(errno, bytes_op, buffer, srb->ctx);

    deallocate(srb);
}

void connection_send_recv_async(connection_t *connection,
                                struct send_recv_tcp_buffer *srb,
                                io_service_t *iosvc) {
    struct async_sr *sr = allocate(sizeof(struct async_sr));
    network_tcp_op_t op;
    NETWORK_TCP_OPERATOR oper;
    io_svc_op_t io_svc_op;

    assert(sr && connection && srb && iosvc &&
           connection->ep_skt.skt >= 0 &&
           connection->ep_skt.ep.ep_type == EPT_TCP);

    op = srb->op;
    oper = TCP_OPERATORS[op].op;
    io_svc_op = TCP_OPERATORS[op].io_svc_op;

    sr->connection = connection;
    sr->iosvc = iosvc;
    sr->srb = srb;

    io_service_post_job(iosvc,
                        connection->ep_skt.skt,
                        io_svc_op,
                        true,
                        send_recv_async_tpl,
                        sr);
}
