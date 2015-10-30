#include "network.h"
#include "io-service.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <assert.h>

typedef ssize_t (*TCP_OPERATOR)(int sockfd, void *buffer, size_t len, int flags);

struct TCP_OPERATION {
    TCP_OPERATOR oper;
    io_svc_op_t iosvc_op;
};

static const struct TCP_OPERATION TCP_OPERATIONS[] = {
    [SRB_OP_SEND] = {
        .oper = send,
        .iosvc_op = IO_SVC_OP_WRITE
    },
    [SRB_OP_RECV] = {
        .oper = recv,
        .iosvc_op = IO_SVC_OP_READ
    }
};

static
void tcp_send_recv_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    srb_t *srb = ctx;
    buffer_t *buffer = srb->buffer;
    size_t bytes_op = srb->bytes_operated;
    ssize_t bytes_op_cur;
    srb_operation_t op = srb->operation.op;
    io_service_t *iosvc = srb->iosvc;
    TCP_OPERATOR oper = TCP_OPERATIONS[op].oper;
    io_svc_op_t io_svc_op = TCP_OPERATIONS[op].iosvc_op;
    ssize_t bytes_op_cur;

    errno = 0;
    bytes_op_cur = (*oper)(srb->aux.dst->skt,
                           buffer_data(buffer) + bytes_op,
                           buffer_size(buffer) - bytes_op,
                           MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytes_op_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(iosvc,
                                srb->aux.dst->skt,
                                io_svc_op,
                                true,
                                tcp_send_recv_async_tpl,
                                srb);
        else {
            if (srb->cb) (*srb->cb)(srb, srb->aux.dst->ep, errno, srb->ctx);
            deallocate(srb);
        }
    }
    else {
        bytes_op += bytes_op_cur;
        srb->bytes_operated = bytes_op;
        if (bytes_op < buffer_size(buffer))
            io_service_post_job(iosvc,
                                srb->aux.dst->skt,
                                io_svc_op,
                                true,
                                tcp_send_recv_async_tpl,
                                srb);
        else {
            if (srb->cb) (*srb->cb)(srb, srb->aux.dst->ep, errno, srb->ctx);
            deallocate(srb);
        }
    }
}

static
void tcp_send_recv_sync(srb_t *srb) {
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    srb_operation_t op;
    TCP_OPERATOR oper;

    assert(srb &&
           srb->aux.dst &&
           srb->aux.dst->skt >= 0 &&
           srb->aux.dst->ep.ep_type == EPT_TCP);

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = TCP_OPERATIONS[op].oper;

    assert(buffer != NULL);

    bytes_op = srb->bytes_operated;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(srb->aux.dst->skt,
                               buffer_data(buffer) + bytes_op,
                               buffer_size(buffer) - bytes_op,
                               MSG_NOSIGNAL);
        if (bytes_op_cur < 0) break;

        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;

    if (srb->cb)
        (*srb->cb)(srb, srb->aux.dst->ep, errno, srb->ctx);

    deallocate(srb);
}

static tcp_send_recv_async(srb_t *srb) {
    assert(srb &&
           srb->aux.dst &&
           srb->aux.dst->skt >= 0 &&
           srb->aux.dst->ep.ep_type == EPT_TCP);
    io_service_post_job(srb->iosvc,
                        srb->aux.dst->skt,
                        TCP_OPERATIONS[srb->operation.op].iosvc_op,
                        true,
                        tcp_send_recv_async_tpl,
                        srb);
}

static
void operate_tcp(srb_t *srb) {
    if (srb->iosvc) tcp_send_recv_sync(srb);
    else tcp_send_recv_async(srb);
}

static
void operate_udp(srb_t *srb) {
    switch (srb->operation.op) {
        case SRB_OP_SEND:
            break;
        case SRB_OP_RECV:
            break;
    }
}

void srb_operate(srb_t *srb) {
    if (!srb) return;
    assert(srb->operation.type < EPT_MAX && srb->operation.op < SRB_OP_MAX);

    switch (srb->operation.type) {
        case EPT_TCP:
            operate_tcp(srb);
            break;
        case EPT_UDP:
            operate_udp(srb);
            break;
    }
}
