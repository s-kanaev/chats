#include "client.h"
#include "memory.h"
#include "endpoint.h"
#include "io-service.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct client_tcp {
    bool connected;
    int reuse_addr;
    pthread_mutexattr_t mtx_attr;
    pthread_mutex_t mutex;
    io_service_t *master;
    char *local_addr;
    char *local_port;
    endpoint_socket_t local;
    endpoint_t remote;
};

static
bool client_tcp_init_socket(client_tcp_t *client) {
    struct addrinfo *addr_info = NULL, *cur_addr, hint;
    int ret, sfd;
    uint32_t addr;
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);

    if (client->local.skt != -1) return false;

    if (client->local_addr == NULL && client->local_port == NULL) {
        sfd = socket(AF_INET,
                     SOCK_STREAM | SOCK_CLOEXEC,
                     0);

        assert(sfd >= 0);

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
            &client->reuse_addr,
            sizeof(client->reuse_addr))) goto fail;

        client->local.ep.ep_type = EPT_TCP;
        client->local.skt = sfd;
        assert(0 == getsockname(sfd, (struct sockaddr *)&local_addr, &len));

        client->local.ep.ep_class = EPC_IP4;
        client->local.ep.ep.ip4.port = ntohs(client->local.ep.addr.ip4.sin_port);
        addr = ntohl(client->local.ep.addr.ip4.sin_addr.s_addr);
        client->local.ep.ep.ip4.addr[0] = addr >> 0x18;
        client->local.ep.ep.ip4.addr[1] = (addr >> 0x10) & 0xff;
        client->local.ep.ep.ip4.addr[2] = (addr >> 0x08) & 0xff;
        client->local.ep.ep.ip4.addr[3] = addr & 0xff;
    }
    else {
        memset(&hint, 0, sizeof(hint));
        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_STREAM;
        hint.ai_protocol = 0;
        hint.ai_flags = AI_V4MAPPED | AI_PASSIVE;
        ret = getaddrinfo(client->local_addr, client->local_port, &hint, &addr_info);
        if (ret != 0) goto fail;

        for (cur_addr = addr_info; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
            sfd = socket(cur_addr->ai_family,
                         cur_addr->ai_socktype | SOCK_CLOEXEC,
                         cur_addr->ai_protocol);

            if (sfd < 0) continue;

            if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                           &client->reuse_addr,
                           sizeof(client->reuse_addr))) goto fail;

            if (!bind(sfd, cur_addr->ai_addr, cur_addr->ai_addrlen)) break;

            shutdown(sfd, SHUT_RDWR);
            close(sfd);
        }

        if (cur_addr == NULL) goto fail;

        client->local.ep.ep_type = EPT_TCP;
        client->local.skt = sfd;
        memcpy(&client->local.ep.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);

        switch (cur_addr->ai_family) {
            case AF_INET:
                client->local.ep.ep_class = EPC_IP4;
                client->local.ep.ep.ip4.port = ntohs(client->local.ep.addr.ip4.sin_port);
                addr = ntohl(client->local.ep.addr.ip4.sin_addr.s_addr);
                client->local.ep.ep.ip4.addr[0] = addr >> 0x18;
                client->local.ep.ep.ip4.addr[1] = (addr >> 0x10) & 0xff;
                client->local.ep.ep.ip4.addr[2] = (addr >> 0x08) & 0xff;
                client->local.ep.ep.ip4.addr[3] = addr & 0xff;
                break;
            case AF_INET6:
                client->local.ep.ep_class = EPC_IP6;
                client->local.ep.ep.ip6.port = ntohs(client->local.ep.addr.ip6.sin6_port);
                memcpy(client->local.ep.ep.ip6.addr,
                       &client->local.ep.addr.ip6.sin6_addr,
                       sizeof(client->local.ep.ep.ip6.addr));
                break;
        }
    }

    if (addr_info) freeaddrinfo(addr_info);
    return true;

fail:
    if (addr_info) freeaddrinfo(addr_info);
    return false;
}

static
void client_tcp_connector(int fd, io_svc_op_t op, void *ctx) {
    struct connector *connector = ctx;
    client_tcp_t *client = connector->host;
    endpoint_t *ep = &client->remote;
    int ret;
    int err;
    socklen_t len = sizeof(err);

    ret = getsockopt(client->local.skt, SOL_SOCKET, SO_ERROR, &err, &len);
    assert(ret == 0);

    client->connected = true;

    if (err) {
        client_tcp_disconnect(client);
        ep = NULL;
    }

    if (connector->connection_cb &&
        !(*connector->connection_cb)(&client->remote, err, connector->connection_ctx))
        client_tcp_disconnect(client);

    deallocate(connector);
}

static
void client_send_recv_sync(struct send_recv_tcp_buffer *srb) {
    client_tcp_t *client;
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_sent_cur;
    network_tcp_op_t op;
    NETWORK_TCP_OPERATOR oper;

    assert(srb != NULL);

    client = srb->host;
    buffer = srb->buffer;
    op = srb->op;
    oper = TCP_OPERATORS[op].op;

    assert(client != NULL);
    assert(buffer != NULL);

    bytes_op = srb->bytes_operated;

    if (!client->connected) return;

    pthread_mutex_lock(&client->mutex);

    while (bytes_op < buffer_size(buffer)) {
        errno = 0;
        bytes_sent_cur = (*oper)(client->local.skt,
                                 buffer_data(buffer) + bytes_op,
                                 buffer_size(buffer) - bytes_op,
                                 MSG_NOSIGNAL);
        if (bytes_sent_cur < 0) break;

        bytes_op += bytes_sent_cur;
    }

    srb->bytes_operated = bytes_op;

    (*srb->cb)(errno, bytes_op, buffer, srb->ctx);

    pthread_mutex_unlock(&client->mutex);

    deallocate(srb);
}

static
void client_send_recv_async(int fd, io_svc_op_t op_, void *ctx) {
    struct send_recv_tcp_buffer *srb = ctx;
    client_tcp_t *client;
    buffer_t *buffer;
    size_t bytes_op;
    ssize_t bytes_op_cur;
    network_tcp_op_t op;
    NETWORK_TCP_OPERATOR oper;
    io_svc_op_t io_svc_op;

    assert(srb != NULL);

    client = srb->host;
    buffer = srb->buffer;
    op = srb->op;
    oper = TCP_OPERATORS[op].op;
    io_svc_op = TCP_OPERATORS[op].io_svc_op;

    assert(client != NULL);
    assert(buffer != NULL);

    bytes_op = srb->bytes_operated;

    if (!client->connected) return;

    pthread_mutex_lock(&client->mutex);

    errno = 0;
    bytes_op_cur = (*oper)(client->local.skt,
                           buffer_data(buffer) + bytes_op,
                           buffer_size(buffer) - bytes_op,
                           MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytes_op_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(client->master,
                                client->local.skt,
                                io_svc_op,
                                true,
                                client_send_recv_async,
                                srb);
        else {
            (*srb->cb)(errno, bytes_op, buffer, srb->ctx);
            deallocate(srb);
        }
    }
    else {
        bytes_op += bytes_op_cur;
        srb->bytes_operated = bytes_op;
        if (bytes_op < buffer_size(buffer))
            io_service_post_job(client->master,
                                client->local.skt,
                                io_svc_op,
                                true,
                                client_send_recv_async,
                                srb);
        else {
            (*srb->cb)(errno, bytes_op, buffer, srb->ctx);
            deallocate(srb);
        }
    }

    pthread_mutex_unlock(&client->mutex);
}

client_tcp_t *client_tcp_init(io_service_t *svc,
                              const char *addr, const char *port,
                              int reuse_addr) {
    client_tcp_t *client = NULL;
    int socket_family, socket_type = SOCK_STREAM | SOCK_CLOEXEC;

    if (svc == NULL) goto fail;

    client = allocate(sizeof(client_tcp_t));

    if (!client) goto fail;

    pthread_mutexattr_init(&client->mtx_attr);
    pthread_mutexattr_settype(&client->mtx_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&client->mutex, &client->mtx_attr);

    client->master = svc;
    client->reuse_addr = reuse_addr;
    client->local_addr = client->local_port = NULL;
    client->remote.ep_type = client->local.ep.ep_type = EPT_NONE;
    client->remote.ep_class = client->local.ep.ep_class = EPC_NONE;
    client->local.skt = -1;

    if (addr) client->local_addr = strdup(addr);
    if (port) client->local_port = strdup(port);

    client->connected = false;
    return client;

fail:
    if (client) {
        if (client->local_addr) free(client->local_addr);
        if (client->local_port) free(client->local_port);
        deallocate(client);
    }
    return NULL;
}

void client_tcp_deinit(client_tcp_t *client) {
    if (!client) return;

    if (client->connected)
        client_tcp_disconnect(client);

    shutdown(client->local.skt, SHUT_RDWR);
    close(client->local.skt);

    pthread_mutex_destroy(&client->mutex);
    pthread_mutexattr_destroy(&client->mtx_attr);

    if (client->local_addr) free(client->local_addr);
    if (client->local_port) free(client->local_port);
    deallocate(client);
}

void client_tcp_disconnect(client_tcp_t *client) {
    if (!client) return;
    if (!client->connected) return;

    shutdown(client->local.skt, SHUT_RDWR);
    close(client->local.skt);
    client->local.skt = -1;
    client->connected = false;
}

void client_tcp_local_ep(client_tcp_t *client, endpoint_t **ep) {
    if (ep == NULL || client == NULL) return;
    if (*ep == NULL) *ep = allocate(sizeof(endpoint_t));

    memcpy(*ep, &client->local.ep, sizeof(client->local.ep));
}

void client_tcp_remote_ep(client_tcp_t *client, endpoint_t **ep) {
    if (ep == NULL || client == NULL) return;
    if (*ep == NULL) *ep = allocate(sizeof(endpoint_t));

    memcpy(*ep, &client->remote.ep, sizeof(client->remote.ep));
}

void client_tcp_connect_sync(client_tcp_t *client,
                             const char *addr, const char *port,
                             tcp_connection_cb_t cb, void *ctx) {
    struct addrinfo *addr_info = NULL, *cur_addr, hint;
    endpoint_t *ep;
    int ret;
    uint32_t local_addr;

    if (!client || !(addr && port)) {
        (*cb)(NULL, EADDRNOTAVAIL, ctx);
        return;
    }
    if (!client_tcp_init_socket(client)) return;

    memset(&hint, 0, sizeof(hint));

    hint.ai_family = client->local.ep.ep_class == EPC_IP4
                      ? AF_INET
                      : AF_INET6;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_flags = AI_V4MAPPED;
    ret = getaddrinfo(addr, port, &hint, &addr_info);
    if (ret != 0) {
        (*cb)(NULL, EADDRNOTAVAIL, ctx);
        return;
    }

    errno = 0;
    for (cur_addr = addr_info; cur_addr != NULL; cur_addr = cur_addr->ai_next)
        if (!connect(client->local.skt, cur_addr->ai_addr, cur_addr->ai_addrlen)) break;

    if (cur_addr == NULL) {
        freeaddrinfo(addr_info);
        if(cb) (*cb)(NULL, errno, ctx);
        return;
    }

    client->remote.ep_type = EPT_TCP;
    memcpy(&client->remote.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);

    switch (cur_addr->ai_family) {
    case AF_INET:
        client->remote.ep_class = EPC_IP4;
        client->remote.ep.ip4.port =
            ntohs(((struct sockaddr_in *)cur_addr->ai_addr)->sin_port);
        local_addr =
            ntohl(((struct sockaddr_in *)cur_addr->ai_addr)->sin_addr.s_addr);
        client->remote.ep.ip4.addr[0] = local_addr >> 0x18;
        client->remote.ep.ip4.addr[1] = (local_addr >> 0x10) & 0xff;
        client->remote.ep.ip4.addr[2] = (local_addr >> 0x08) & 0xff;
        client->remote.ep.ip4.addr[3] = local_addr & 0xff;
        break;

    case AF_INET6:
        client->remote.ep_class = EPC_IP6;
        client->remote.ep.ip6.port =
            ntohs(((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_port);
        memcpy(&client->remote.ep.ip6.addr,
               &((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_addr,
               sizeof(client->remote.ep.ip6.addr));
        break;
    }

    client->connected = true;
    ep = &client->remote;

    if (errno) {
        client_tcp_disconnect(client);
        ep = NULL;
    }

    if ((cb && !(*cb)(ep, errno, ctx)))
        client_tcp_disconnect(client);

    if (addr_info) freeaddrinfo(addr_info);
}

void client_tcp_connect_async(client_tcp_t *client,
                              const char *addr, const char *port,
                              tcp_connection_cb_t cb, void *ctx) {
    int flags;
    int errno_old;
    struct addrinfo *addr_info = NULL, *cur_addr, hint;
    int ret;
    struct connector *connector;
    uint32_t local_addr;

    if (!client || !addr || !port) return;
    if (!client_tcp_init_socket(client)) return;

    connector = allocate(sizeof(struct connector));
    assert(connector);

    connector->host = client;
    connector->connection_cb = cb;
    connector->connection_ctx = ctx;

    flags = fcntl(client->local.skt, F_GETFL);
    assert(flags >= 0);

    if (fcntl(client->local.skt, F_SETFL, flags | O_NONBLOCK)) {
        if (cb) (*cb)(NULL, errno, ctx);
        return;
    }

    memset(&hint, 0, sizeof(hint));
    hint.ai_family = client->local.ep.ep_class == EPC_IP4
                      ? AF_INET
                      : AF_INET6;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_flags = AI_V4MAPPED;
    ret = getaddrinfo(addr, port, &hint, &addr_info);
    if (ret != 0) {
        (*cb)(NULL, EADDRNOTAVAIL, ctx);
        return;
    }

    errno = 0;
    for (cur_addr = addr_info; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
        ret = connect(client->local.skt, cur_addr->ai_addr, cur_addr->ai_addrlen);
        if (ret == 0 || errno == EINPROGRESS) {
            memcpy(&client->remote.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);
            switch (cur_addr->ai_addrlen) {
                case sizeof(struct sockaddr_in):
                    client->remote.ep_class = EPC_IP4;
                    client->remote.ep.ip4.port =
                        ntohs(((struct sockaddr_in *)cur_addr->ai_addr)->sin_port);
                    local_addr =
                        ntohl(((struct sockaddr_in *)cur_addr->ai_addr)->sin_addr.s_addr);
                    client->remote.ep.ip4.addr[0] = local_addr >> 0x18;
                    client->remote.ep.ip4.addr[1] = (local_addr >> 0x10) & 0xff;
                    client->remote.ep.ip4.addr[2] = (local_addr >> 0x08) & 0xff;
                    client->remote.ep.ip4.addr[3] = local_addr & 0xff;
                    break;
                case sizeof(struct sockaddr_in6):
                    client->remote.ep_class = EPC_IP6;
                    client->remote.ep.ip6.port =
                        ntohs(((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_port);
                    memcpy(&client->remote.ep.ip6.addr,
                           &((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_addr,
                           sizeof(client->remote.ep.ip6.addr));
                    break;
                default:
                    assert(0);
                    break;
            }

            io_service_post_job(
                client->master,
                client->local.skt,
                IO_SVC_OP_WRITE,
                true,
                client_tcp_connector,
                connector);
            break;
        }
    }

    if (cur_addr == NULL) {
        if(cb) (*cb)(NULL, errno, ctx);
        deallocate(connector);
    }

    if (addr_info) freeaddrinfo(addr_info);
}

void client_tcp_recv_sync(client_tcp_t *client, buffer_t *buffer,
                          network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *rb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(rb != NULL);

    rb->buffer = buffer;
    rb->host = client;
    rb->bytes_operated = 0;
    rb->cb = cb;
    rb->ctx = ctx;
    rb->op = NETWORK_TCP_OP_RECEIVE;

    client_send_recv_sync(rb);
}

void client_tcp_recv_async(client_tcp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *rb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(rb != NULL);

    rb->buffer = buffer;
    rb->host = client;
    rb->bytes_operated = 0;
    rb->cb = cb;
    rb->ctx = ctx;
    rb->op = NETWORK_TCP_OP_RECEIVE;

    io_service_post_job(client->master,
                        client->local.skt,
                        IO_SVC_OP_READ,
                        true,
                        client_send_recv_async,
                        rb);
}

void client_tcp_send_sync(client_tcp_t *client, buffer_t *buffer,
                          network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *sb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(sb != NULL);

    sb->buffer = buffer;
    sb->host = client;
    sb->bytes_operated = 0;
    sb->cb = cb;
    sb->ctx = ctx;
    sb->op = NETWORK_TCP_OP_SEND;

    client_send_recv_sync(sb);
}

void client_tcp_send_async(client_tcp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *sb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(sb != NULL);

    sb->buffer = buffer;
    sb->host = client;
    sb->bytes_operated = 0;
    sb->cb = cb;
    sb->ctx = ctx;
    sb->op = NETWORK_TCP_OP_SEND;

    io_service_post_job(client->master,
                        client->local.skt,
                        IO_SVC_OP_WRITE,
                        true,
                        client_send_recv_async,
                        sb);
}
