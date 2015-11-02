#include "network.h"
#include "io-service.h"
#include "memory.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <linux/sockios.h>

#include <errno.h>
#include <assert.h>

typedef ssize_t (*NET_OPERATOR)(int sockfd, struct msghdr *msg, int flags);

struct NET_OPERATION {
    NET_OPERATOR oper;
    io_svc_op_t iosvc_op;
};

static const struct NET_OPERATION NET_OPERATIONS[] = {
    [SRB_OP_SEND] = {
        .oper = sendmsg,
        .iosvc_op = IO_SVC_OP_WRITE
    },
    [SRB_OP_RECV] = {
        .oper = recvmsg,
        .iosvc_op = IO_SVC_OP_READ
    }
};

static
void tcp_send_recv_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    srb_t *srb = ctx;
    buffer_t *buffer = srb->buffer;
    size_t bytes_op = srb->bytes_operated;
    srb_operation_t op = srb->operation.op;
    io_service_t *iosvc = srb->iosvc;
    TCP_OPERATOR oper = NET_OPERATIONS[op].oper;
    io_svc_op_t io_svc_op = NET_OPERATIONS[op].iosvc_op;
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
void udp_send_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    srb_t *srb = ctx;
    buffer_t *buffer = srb->buffer;
    size_t bytes_op = srb->bytes_operated;
    io_service_t *iosvc = srb->iosvc;
    ssize_t bytes_op_cur;

    errno = 0;
    bytes_op_cur = sendto(srb->aux.dst->skt,
                          buffer_data(buffer) + bytes_op,
                          buffer_size(buffer) - bytes_op,
                          MSG_DONTWAIT | MSG_NOSIGNAL,
                          (struct sockaddr *)&srb->aux.dst->ep.addr,
                          srb->aux.dst->ep.ep_class == EPC_IP4
                           ? sizeof(srb->aux.dst->ep.addr.ip4)
                           : sizeof(srb->aux.dst->ep.addr.ip6));

    if (bytes_op_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(iosvc,
                                srb->aux.dst->skt,
                                IO_SVC_OP_WRITE,
                                true,
                                udp_send_async_tpl,
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
                                IO_SVC_OP_WRITE,
                                true,
                                udp_send_async_tpl,
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
    oper = NET_OPERATIONS[op].oper;

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

static
void tcp_send_recv_async(srb_t *srb) {
    assert(srb &&
           srb->aux.dst &&
           srb->aux.dst->skt >= 0 &&
           srb->aux.dst->ep.ep_type == EPT_TCP);
    io_service_post_job(srb->iosvc,
                        srb->aux.dst.skt,
                        NET_OPERATIONS[srb->operation.op].iosvc_op,
                        true,
                        tcp_send_recv_async_tpl,
                        srb);
}

static
void udp_send_sync(srb_t *srb) {
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;

    assert(srb &&
           srb->aux.dst &&
           srb->aux.dst->skt >= 0 &&
           srb->aux.dst->ep.ep_type == EPT_UDP);

    buffer = srb->buffer;

    assert(buffer != NULL);

    bytes_op = srb->bytes_operated;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = sendto(srb->aux.dst->skt,
                              buffer_data(buffer) + bytes_op,
                              buffer_size(buffer) - bytes_op,
                              MSG_NOSIGNAL,
                              (struct sockaddr *)&srb->aux.dst->ep.addr,
                              srb->aux.dst->ep.ep_class == EPC_IP4
                               ? sizeof(srb->aux.dst->ep.addr.ip4)
                               : sizeof(srb->aux.dst->ep.addr.ip6));
        if (bytes_op_cur < 0) break;

        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;

    if (srb->cb)
        (*srb->cb)(srb, srb->aux.dst->ep, errno, srb->ctx);

    deallocate(srb);
}

static
void udp_send_async(srb_t *srb) {
    assert(srb &&
           srb->aux.dst &&
           srb->aux.dst->skt >= 0 &&
           srb->aux.dst->ep.ep_type == EPT_UDP);
    io_service_post_job(srb->iosvc,
                        srb->aux.dst->skt,
                        IO_SVC_OP_WRITE,
                        true,
                        udp_send_async_tpl,
                        srb);
}

static
void udp_recv_sync(srb_t *srb) {
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    socklen_t len;

    assert(srb &&
           srb->aux.src &&
           srb->aux.src->skt >= 0 &&
           srb->aux.src->ep.ep_type == EPT_UDP);

    len = sizeof(srb->aux.src->ep.addr);

    buffer = srb->buffer;
    assert(buffer != NULL);

    bytes_op = srb->bytes_operated;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = recvfrom(srb->aux.dst->skt,
                                buffer_data(buffer) + bytes_op,
                                buffer_size(buffer) - bytes_op,
                                MSG_NOSIGNAL,
                                (struct sockaddr *)&srb->aux.src->ep.addr,
                                &len);

        if (bytes_op_cur < 0) break;

        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;

    if (srb->cb)
        (*srb->cb)(srb, srb->aux.dst->ep, errno, srb->ctx);

    deallocate(srb);
}

static
void operate_tcp(srb_t *srb) {
    if (srb->iosvc) tcp_send_recv_async(srb);
    else tcp_send_recv_sync(srb);
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

/*****************************************/

static
void srb_recv_async(srb_t *srb) {
    /* TODO */
}

static
void srb_recv_sync(srb_t *srb) {
    buffer_t **buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    srb_operation_t op;
    NET_OPERATOR oper;
    int bytes_pending;

    assert(srb && srb->aux.src.skt >= 0);

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = NET_OPERATIONS[op].oper;

    assert(buffer != NULL && *buffer != NULL);
    assert(0 == ioctl(srb->aux.src.skt, SIOCINQ, &bytes_pending));
    assert(true == buffer_resize(buffer, bytes_pending));

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = &srb->aux.dst.ep.addr;
    srb->mhdr.msg_namelen = sizeof(srb->aux.dst.ep.addr);

    srb->vec.iov_base = buffer_data(*buffer);
    srb->vec.iov_len = bytes_pending;

    bytes_op = srb->bytes_operated = 0;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(srb->aux.src.skt, &srb->mhdr, MSG_NOSIGNAL);
        if (bytes_op_cur < 0) break;
        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;

    if (srb->cb)
        (*srb->cb)(srb, srb->aux.dst->ep, errno, srb->ctx);

    deallocate(srb);

    /* TODO */
}

static
void srb_send_async(srb_t *srb) {
    /* TODO */
}

static
void srb_send_sync(srb_t *srb) {
    /* TODO */
}

static
void srb_recv(srb_t *srb) {
}

static
void srb_send(srb_t *srb) {
}

void srb_operate(srb_t *srb) {
    if (!srb) return;
    assert(srb->operation.type < EPT_MAX && srb->operation.op < SRB_OP_MAX);

    switch (srb->operation.op) {
        case SRB_OP_RECV:
            srb_recv(srb);
            break;
        case SRB_OP_SEND:
            srb_send(srb);
            break;
    }

    switch (srb->operation.type) {
        case EPT_TCP:
            operate_tcp(srb);
            break;
        case EPT_UDP:
            operate_udp(srb);
            break;
    }
}

void send_recv_cb(srb_t *srb, endpoint_t ep, int err, void *ctx) {
#warning "Make sure src_t is allocated along with srb_t"
    src_t *src = ctx;
    if (src->cb)
        (*src->cb)(ep, err, srb->bytes_operated, srb->buffer, src->ctx);
}
