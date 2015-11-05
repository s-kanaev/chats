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
    int ioctl_request;
    io_svc_op_t iosvc_op;
};

static const struct NET_OPERATION NET_OPERATIONS[] = {
    [SRB_OP_SEND] = {
        .oper = sendmsg,
        .ioctl_request = SIOCOUTQ,
        .iosvc_op = IO_SVC_OP_WRITE
    },
    [SRB_OP_RECV] = {
        .oper = recvmsg,
        .ioctl_request = SIOCINQ,
        .iosvc_op = IO_SVC_OP_READ
    }
};

typedef void (*OPERATOR)(srb_t *srb);
#if 0
static OPERATOR tcp_send_recv_async;
static OPERATOR tcp_send_recv_sync;
static OPERATOR udp_send_async;
static OPERATOR udp_send_sync;
static OPERATOR udp_recv_async;
static OPERATOR udp_recv_sync;
#else
static void tcp_send_recv_async(srb_t *srb);
static void tcp_send_recv_sync(srb_t *srb);
static void udp_send_async(srb_t *srb);
static void udp_send_sync(srb_t *srb);
static void udp_recv_async(srb_t *srb);
static void udp_recv_sync(srb_t *srb);
#endif

#define OP_SYNC 0
#define OP_ASYNC 1

#define OPERATOR_IDX(proto, op, sync) \
    (\
     (((proto) & 0x01) << 0x02) | \
     (((op) & 0x01) << 0x01) | \
     (((sync) & 0x01)) \
    )

static const OPERATOR OPERATORS[] = {
    [OPERATOR_IDX(EPT_TCP, SRB_OP_SEND, OP_SYNC)] = tcp_send_recv_sync,
    [OPERATOR_IDX(EPT_TCP, SRB_OP_SEND, OP_ASYNC)] = tcp_send_recv_async,
    [OPERATOR_IDX(EPT_TCP, SRB_OP_RECV, OP_SYNC)] = tcp_send_recv_sync,
    [OPERATOR_IDX(EPT_TCP, SRB_OP_RECV, OP_ASYNC)] = tcp_send_recv_async,

    [OPERATOR_IDX(EPT_UDP, SRB_OP_SEND, OP_SYNC)] = udp_send_sync,
    [OPERATOR_IDX(EPT_UDP, SRB_OP_SEND, OP_ASYNC)] = udp_send_async,
    [OPERATOR_IDX(EPT_UDP, SRB_OP_RECV, OP_SYNC)] = udp_recv_sync,
    [OPERATOR_IDX(EPT_UDP, SRB_OP_RECV, OP_ASYNC)] = udp_recv_async,
};

/***************** functions *********************/
static
void tcp_send_recv_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    srb_t *srb = ctx;
    buffer_t *buffer = srb->buffer;
    size_t bytes_op = srb->bytes_operated;
    srb_operation_t op = srb->operation.op;
    io_service_t *iosvc = srb->iosvc;
    NET_OPERATOR oper = NET_OPERATIONS[op].oper;
    io_svc_op_t io_svc_op = NET_OPERATIONS[op].iosvc_op;
    ssize_t bytes_op_cur;
    int more_bytes;
    endpoint_t *ep_ptr;

    errno = 0;
    srb->vec.iov_base = buffer_data(buffer) + bytes_op;
    srb->vec.iov_len = buffer_size(buffer) - bytes_op;
    bytes_op_cur = (*oper)(fd,
                           &srb->mhdr,
                           MSG_NOSIGNAL | MSG_DONTWAIT);

    if (bytes_op_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(iosvc,
                                fd,
                                io_svc_op,
                                true,
                                tcp_send_recv_async_tpl,
                                srb);
        else {
            ep_ptr = op == SRB_OP_SEND
                            ? &srb->aux.dst.ep
                            : &srb->aux.src.ep;
            if (srb->cb)
                (*srb->cb)(*ep_ptr, errno, bytes_op, more_bytes, buffer, srb->ctx);
            deallocate(srb);
        }
    }
    else {
        bytes_op += bytes_op_cur;
        srb->bytes_operated = bytes_op;
        if (bytes_op < buffer_size(buffer))
            io_service_post_job(iosvc,
                                fd,
                                io_svc_op,
                                true,
                                tcp_send_recv_async_tpl,
                                srb);
        else {
            assert(0 == ioctl(fd, NET_OPERATIONS[op].ioctl_request, &more_bytes));
            ep_ptr = op == SRB_OP_SEND
                            ? &srb->aux.dst.ep
                            : &srb->aux.src.ep;
            if (srb->cb)
                (*srb->cb)(*ep_ptr, errno, bytes_op, more_bytes, buffer, srb->ctx);
            deallocate(srb);
        }
    }
}

static
void udp_send_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    srb_t *srb = ctx;
    buffer_t *buffer = srb->buffer;
    size_t bytes_op = srb->bytes_operated;
    srb_operation_t op = srb->operation.op;
    io_service_t *iosvc = srb->iosvc;
    NET_OPERATOR oper = NET_OPERATIONS[op].oper;
    io_svc_op_t io_svc_op = NET_OPERATIONS[op].iosvc_op;
    ssize_t bytes_op_cur;
    int more_bytes;

    errno = 0;
    srb->vec.iov_base = buffer_data(buffer) + bytes_op;
    srb->vec.iov_len = buffer_size(buffer) - bytes_op;
    bytes_op_cur = (*oper)(fd,
                           &srb->mhdr,
                           MSG_NOSIGNAL | MSG_DONTWAIT);

    if (bytes_op_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(iosvc,
                                fd,
                                io_svc_op,
                                true,
                                udp_send_async_tpl,
                                srb);
        else {
            if (srb->cb)
                (*srb->cb)(srb->aux.dst.ep, errno, bytes_op, more_bytes, buffer, srb->ctx);
            deallocate(srb);
        }
    }
    else {
        bytes_op += bytes_op_cur;
        srb->bytes_operated = bytes_op;
        if (bytes_op < buffer_size(buffer))
            io_service_post_job(iosvc,
                                fd,
                                io_svc_op,
                                true,
                                udp_send_async_tpl,
                                srb);
        else {
            assert(0 == ioctl(fd, NET_OPERATIONS[op].ioctl_request, &more_bytes));
            if (srb->cb)
                (*srb->cb)(srb->aux.dst.ep, errno, bytes_op, more_bytes, buffer, srb->ctx);
            deallocate(srb);
        }
    }
}

static
void udp_recv_async_tpl(int fd, io_svc_op_t op_, void *ctx) {
    srb_t *srb = ctx;
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    srb_operation_t op;
    NET_OPERATOR oper;
    int bytes_pending;

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = NET_OPERATIONS[op].oper;

    assert(0 == ioctl(srb->aux.src.skt, NET_OPERATIONS[op].ioctl_request, &bytes_pending));
    if (bytes_pending > buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(srb->aux.src.skt,
                               &srb->mhdr,
                               MSG_NOSIGNAL | MSG_PEEK);

        if (srb->cb)
            (*srb->cb)(srb->aux.src.ep, errno ? errno : NSRCE_BUFFER_TOO_SMALL,
                       bytes_op_cur, bytes_pending, buffer, srb->ctx);

        deallocate(srb);
    }

    bytes_op = 0;

    errno = 0;
    bytes_op_cur = (*oper)(srb->aux.src.skt,
                           &srb->mhdr,
                           MSG_NOSIGNAL);
    if (bytes_op_cur >= 0) bytes_op += bytes_op_cur;

    srb->bytes_operated = bytes_op;

    translate_endpoint(&srb->aux.src.ep);

    if (srb->cb)
        (*srb->cb)(srb->aux.src.ep, errno, bytes_op, bytes_pending, buffer, srb->ctx);

    deallocate(srb);
}

static
void tcp_send_recv_sync(srb_t *srb) {
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    srb_operation_t op;
    NET_OPERATOR oper;
    int more_bytes;
    endpoint_socket_t *ep_skt_ptr;

    assert(srb);

    ep_skt_ptr = op == SRB_OP_SEND
                  ? &srb->aux.dst
                  : &srb->aux.src;

    assert(ep_skt_ptr->skt >= 0 &&
               ep_skt_ptr->ep.ep_type == EPT_TCP);

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = NET_OPERATIONS[op].oper;

    assert(buffer != NULL);

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = NULL;
    srb->mhdr.msg_namelen = 0;

    srb->vec.iov_base = buffer_data(buffer);
    srb->vec.iov_len = buffer_size(buffer);

    bytes_op = srb->bytes_operated = 0;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(ep_skt_ptr->skt,
                               &srb->mhdr,
                               MSG_NOSIGNAL);
        if (bytes_op_cur < 0) break;
        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;
    assert(0 == ioctl(ep_skt_ptr->skt, NET_OPERATIONS[op].ioctl_request, &more_bytes));

    if (srb->cb)
        (*srb->cb)(ep_skt_ptr->ep, errno, bytes_op, more_bytes, buffer, srb->ctx);

    deallocate(srb);
}

static
void tcp_send_recv_async(srb_t *srb) {
    buffer_t *buffer;
    endpoint_socket_t *ep_skt_ptr;

    assert(srb);

    ep_skt_ptr = srb->operation.op == SRB_OP_SEND
                  ? &srb->aux.dst
                  : &srb->aux.src;

    assert(ep_skt_ptr->skt >= 0 && ep_skt_ptr->ep.ep_type == EPT_TCP);

    buffer = srb->buffer;

    assert(buffer != NULL);

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = NULL;
    srb->mhdr.msg_namelen = 0;

    srb->vec.iov_base = buffer_data(buffer);
    srb->vec.iov_len = buffer_size(buffer);

    srb->bytes_operated = 0;

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
    srb_operation_t op;
    NET_OPERATOR oper;
    int more_bytes;

    assert(srb &&
           srb->aux.dst.skt >= 0 &&
           srb->aux.dst.ep.ep_type == EPT_UDP);

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = NET_OPERATIONS[op].oper;

    assert(buffer != NULL);

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = (struct sockaddr *)&srb->aux.dst.ep.addr;
    srb->mhdr.msg_namelen = srb->aux.dst.ep.ep_class == EPC_IP4
                             ? sizeof(struct sockaddr_in)
                             : sizeof(struct sockaddr_in6);

    srb->vec.iov_base = buffer_data(buffer);
    srb->vec.iov_len = buffer_size(buffer);

    bytes_op = srb->bytes_operated = 0;

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(srb->aux.dst.skt,
                               &srb->mhdr,
                               MSG_NOSIGNAL);
        if (bytes_op_cur < 0) break;
        bytes_op += bytes_op_cur;
    }

    srb->bytes_operated = bytes_op;
    assert(0 == ioctl(srb->aux.dst.skt, NET_OPERATIONS[op].ioctl_request, &more_bytes));

    if (srb->cb)
        (*srb->cb)(srb->aux.dst.ep, errno, bytes_op, more_bytes, buffer, srb->ctx);

    deallocate(srb);
}

static
void udp_send_async(srb_t *srb) {
    buffer_t *buffer;
    assert(srb &&
           srb->aux.dst.skt >= 0 &&
           srb->aux.dst.ep.ep_type == EPT_UDP);

    buffer = srb->buffer;

    assert(buffer != NULL);

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = (struct sockaddr *)&srb->aux.dst.ep.addr;
    srb->mhdr.msg_namelen = srb->aux.dst.ep.ep_class == EPC_IP4
                             ? sizeof(struct sockaddr_in)
                             : sizeof(struct sockaddr_in6);

    srb->vec.iov_base = buffer_data(buffer);
    srb->vec.iov_len = buffer_size(buffer);

    srb->bytes_operated = 0;

    io_service_post_job(srb->iosvc,
                        srb->aux.dst.skt,
                        NET_OPERATIONS[srb->operation.op].iosvc_op,
                        true,
                        udp_send_async_tpl,
                        srb);
}

static
void udp_recv_sync(srb_t *srb) {
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    srb_operation_t op;
    NET_OPERATOR oper;
    int bytes_pending;

    assert(srb &&
           srb->aux.src.skt >= 0 &&
           srb->aux.src.ep.ep_type == EPT_UDP);

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = NET_OPERATIONS[op].oper;

    assert(buffer != NULL);

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = (struct sockaddr *)&srb->aux.src.ep.addr;
    srb->mhdr.msg_namelen = sizeof(srb->aux.src.ep.addr);

    srb->vec.iov_base = buffer_data(buffer);
    srb->vec.iov_len = buffer_size(buffer);

    bytes_op = srb->bytes_operated = 0;

    assert(0 == ioctl(srb->aux.src.skt, NET_OPERATIONS[op].ioctl_request, &bytes_pending));
    if (bytes_pending > buffer_size(buffer)) {
        errno = 0;
        bytes_op_cur = (*oper)(srb->aux.src.skt,
                               &srb->mhdr,
                               MSG_NOSIGNAL | MSG_PEEK);

        if (srb->cb)
            (*srb->cb)(srb->aux.src.ep, errno ? errno : NSRCE_BUFFER_TOO_SMALL,
                       bytes_op_cur, bytes_pending, buffer, srb->ctx);

        deallocate(srb);
    }

    errno = 0;
    bytes_op_cur = (*oper)(srb->aux.src.skt,
                           &srb->mhdr,
                           MSG_NOSIGNAL);
    if (bytes_op_cur >= 0) bytes_op += bytes_op_cur;

    srb->bytes_operated = bytes_op;

    translate_endpoint(&srb->aux.src.ep);

    if (srb->cb)
        (*srb->cb)(srb->aux.src.ep, errno, bytes_op, bytes_pending, buffer, srb->ctx);

    deallocate(srb);
}

static
void udp_recv_async(srb_t *srb) {
    buffer_t *buffer;
    srb_operation_t op;
    NET_OPERATOR oper;
    int bytes_pending;

    assert(srb &&
           srb->aux.src.skt >= 0 &&
           srb->aux.src.ep.ep_type == EPT_UDP);

    buffer = srb->buffer;
    op = srb->operation.op;
    oper = NET_OPERATIONS[op].oper;

    assert(buffer != NULL);

    srb->mhdr.msg_iovlen = 1;
    srb->mhdr.msg_iov = &srb->vec;
    srb->mhdr.msg_control = NULL;
    srb->mhdr.msg_controllen = 0;
    srb->mhdr.msg_flags = 0;
    srb->mhdr.msg_name = (struct sockaddr *)&srb->aux.src.ep.addr;
    srb->mhdr.msg_namelen = sizeof(srb->aux.src.ep.addr);

    srb->vec.iov_base = buffer_data(buffer);
    srb->vec.iov_len = buffer_size(buffer);

    srb->bytes_operated = 0;

    io_service_post_job(srb->iosvc,
                        srb->aux.dst.skt,
                        NET_OPERATIONS[srb->operation.op].iosvc_op,
                        true,
                        udp_recv_async_tpl,
                        srb);
}

void srb_operate(srb_t *srb) {
    OPERATOR op;

    if (!srb) return;
    assert(srb->operation.type < EPT_MAX && srb->operation.op < SRB_OP_MAX);

    op = OPERATORS[OPERATOR_IDX(srb->operation.type,
                                srb->operation.op,
                                srb->iosvc != NULL)];

    (*op)(srb);
}
