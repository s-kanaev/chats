#include "server.h"
#include "memory.h"
#include "endpoint.h"
#include "connection/connection.h"
#include "io-service.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct oto_server_tcp {
    bool connected;
    int reuse_addr;
    pthread_mutexattr_t mtx_attr;
    pthread_mutex_t mutex;
    io_service_t *master;
    endpoint_socket_t local;
    connection_t remote;
};

static
void oto_tcp_acceptor(int fd, io_svc_op_t op, void *ctx) {
    struct connection_acceptor *acceptor = ctx;
    oto_server_tcp_t *server = acceptor->host;
    uint32_t addr;
    socklen_t len;
    struct sockaddr *dest_addr;
    int afd;

    pthread_mutex_lock(&server->mutex);

    server->remote.host = server;

    errno = 0;
    switch (server->local.ep.ep_class) {
        case EPC_IP4:
            len = sizeof(server->remote.ep_skt.ep.addr.ip4);
            dest_addr = (struct sockaddr *)&server->remote.ep_skt.ep.addr.ip4;
            break;
        case EPC_IP6:
            len = sizeof(server->remote.ep_skt.ep.addr.ip6);
            dest_addr = (struct sockaddr *)&server->remote.ep_skt.ep.addr.ip6;
            break;
        default:
            assert(0);
            break;
    }

    afd = accept(server->local.skt, dest_addr, &len);
    assert(afd >= 0);

    server->connected = true;
    server->remote.ep_skt.skt = afd;

    switch (len) {
        case sizeof(struct sockaddr_in):
            server->remote.ep_skt.ep.ep_class = EPC_IP4;
            server->remote.ep_skt.ep.ep.ip4.port = ntohs(server->remote.ep_skt.ep.addr.ip4.sin_port);
            addr = ntohl(server->remote.ep_skt.ep.addr.ip4.sin_addr.s_addr);
            server->remote.ep_skt.ep.ep.ip4.addr[0] = addr >> 0x18;
            server->remote.ep_skt.ep.ep.ip4.addr[1] = (addr >> 0x10) & 0xff;
            server->remote.ep_skt.ep.ep.ip4.addr[2] = (addr >> 0x08) & 0xff;
            server->remote.ep_skt.ep.ep.ip4.addr[3] = addr & 0xff;
            break;
        case sizeof(struct sockaddr_in6):
            server->remote.ep_skt.ep.ep_class = EPC_IP6;
            server->remote.ep_skt.ep.ep.ip6.port = ntohs(server->remote.ep_skt.ep.addr.ip6.sin6_port);
            memcpy(server->remote.ep_skt.ep.ep.ip6.addr,
                   &server->remote.ep_skt.ep.addr.ip6.sin6_addr,
                   sizeof(server->remote.ep_skt.ep.ep.ip6.addr));
            break;
        default:
            assert(0);
            break;
    }

    if (!(*acceptor->connection_cb)(&server->remote,
                                    errno,
                                    acceptor->connection_ctx)) {
        shutdown(server->remote.ep_skt.skt, SHUT_RDWR);
        close(server->remote.ep_skt.skt);
        server->connected = false;
    }

    deallocate(ctx);
    pthread_mutex_unlock(&server->mutex);
}

oto_server_tcp_t *oto_server_tcp_init(io_service_t *svc,
                                      const char *addr, const char *port,
                                      int reuse_addr) {
    oto_server_tcp_t *server = NULL;
    int ret;
    int sfd;
    struct addrinfo *addr_info = NULL, *cur_addr;
    struct addrinfo hint;
    int socket_family;
    uint32_t addr_ip4;

    if (svc == NULL || addr == NULL || port == NULL) goto fail;

    server = allocate(sizeof(oto_server_tcp_t));

    if (!server) goto fail;

    pthread_mutexattr_init(&server->mtx_attr);
    pthread_mutexattr_settype(&server->mtx_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&server->mutex, &server->mtx_attr);

    server->master = svc;
    server->reuse_addr = reuse_addr;

    memset(&server->remote, 0, sizeof(server->remote));

    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_flags = AI_V4MAPPED | AI_PASSIVE;
    ret = getaddrinfo(addr, port, &hint, &addr_info);
    if (ret != 0) goto fail;

    for (cur_addr = addr_info; cur_addr != NULL; cur_addr = cur_addr->ai_next) {
        sfd = socket(cur_addr->ai_family,
                     cur_addr->ai_socktype | SOCK_CLOEXEC,
                     cur_addr->ai_protocol);

        if (sfd < 0) continue;

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                       &server->reuse_addr,
                       sizeof(server->reuse_addr))) goto fail_socket;

        if (!bind(sfd, cur_addr->ai_addr, cur_addr->ai_addrlen)) break;

        shutdown(sfd, SHUT_RDWR);
        close(sfd);
    }

    if (cur_addr == NULL) goto fail;

    server->local.ep.ep_type = EPT_TCP;
    server->local.skt = sfd;
    memcpy(&server->local.ep.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);

    switch (cur_addr->ai_family) {
        case AF_INET:
            server->local.ep.ep_class = EPC_IP4;
            server->local.ep.ep.ip4.port = ntohs(server->local.ep.addr.ip4.sin_port);
            addr_ip4 = ntohl(server->local.ep.addr.ip4.sin_addr.s_addr);
            server->local.ep.ep.ip4.addr[0] = addr_ip4 >> 0x18;
            server->local.ep.ep.ip4.addr[1] = (addr_ip4 >> 0x10) & 0xff;
            server->local.ep.ep.ip4.addr[2] = (addr_ip4 >> 0x08) & 0xff;
            server->local.ep.ep.ip4.addr[3] = addr_ip4 & 0xff;
            break;
        case AF_INET6:
            server->local.ep.ep_class = EPC_IP6;
            server->local.ep.ep.ip6.port = ntohs(server->local.ep.addr.ip6.sin6_port);
            memcpy(server->local.ep.ep.ip6.addr,
                   &server->local.ep.addr.ip6.sin6_addr,
                   sizeof(server->local.ep.ep.ip6.addr));
            break;
    }

    if (listen(server->local.skt, 1)) goto fail_socket;

    server->connected = false;

    freeaddrinfo(addr_info);
    return server;

fail_socket:
    shutdown(server->local.skt, SHUT_RDWR);
    close(server->local.skt);

fail:
    if (addr_info) freeaddrinfo(addr_info);
    if (server) deallocate(server);
    return NULL;
}

void oto_server_tcp_deinit(oto_server_tcp_t *server) {
    if (!server) return;

    if (server->connected)
        oto_server_tcp_disconnect(server);

    shutdown(server->local.skt, SHUT_RDWR);
    close(server->local.skt);

    pthread_mutex_destroy(&server->mutex);
    pthread_mutexattr_destroy(&server->mtx_attr);

    deallocate(server);
}

void oto_server_tcp_disconnect(oto_server_tcp_t *server) {
    if (!server) return;

    shutdown(server->remote.ep_skt.skt, SHUT_RDWR);
    close(server->remote.ep_skt.skt);
}

void oto_server_tcp_listen_sync(oto_server_tcp_t *server,
                                tcp_connection_cb_t cb, void *ctx) {
    struct connection_acceptor *acceptor;
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (server->connected) {
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    acceptor = allocate(sizeof(struct connection_acceptor));
    assert(acceptor != NULL);

    acceptor->host = server;
    acceptor->connection_cb = cb;
    acceptor->connection_ctx = ctx;

    oto_tcp_acceptor(server->local.skt, IO_SVC_OP_READ, acceptor);
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_listen_async(oto_server_tcp_t *server,
                                 tcp_connection_cb_t cb, void *ctx) {
    struct connection_acceptor *acceptor;
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (server->connected) {
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    acceptor = allocate(sizeof(struct connection_acceptor));
    assert(acceptor != NULL);

    acceptor->host = server;
    acceptor->connection_cb = cb;
    acceptor->connection_ctx = ctx;

    io_service_post_job(server->master,
                        server->local.skt,
                        IO_SVC_OP_READ,
                        true,
                        oto_tcp_acceptor,
                        acceptor);
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_local_ep(oto_server_tcp_t *server, endpoint_socket_t *ep) {
    if (!server || !ep) return;

    pthread_mutex_lock(&server->mutex);
    memcpy(ep, &server->local, sizeof(*ep));
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_remote_ep(oto_server_tcp_t *server, endpoint_socket_t *ep) {
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (server->connected)
        memcpy(ep, &server->remote.ep_skt, sizeof(*ep));
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_send_sync(oto_server_tcp_t *server,
                              buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *sb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(sb != NULL);

    sb->buffer = buffer;
    sb->host = server;
    sb->bytes_operated = 0;
    sb->cb = cb;
    sb->ctx = ctx;
    sb->op = NETWORK_TCP_OP_SEND;

    connection_send_recv_sync(&server->remote, sb);
}

void oto_server_tcp_send_async(oto_server_tcp_t *server,
                               buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *sb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(sb != NULL);

    sb->buffer = buffer;
    sb->host = server;
    sb->bytes_operated = 0;
    sb->cb = cb;
    sb->ctx = ctx;
    sb->op = NETWORK_TCP_OP_SEND;

    connection_send_recv_async(&server->remote, sb, server->master);
}

void oto_server_tcp_recv_sync(oto_server_tcp_t *server,
                              buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *rb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(rb != NULL);

    rb->buffer = buffer;
    rb->host = server;
    rb->bytes_operated = 0;
    rb->cb = cb;
    rb->ctx = ctx;
    rb->op = NETWORK_TCP_OP_RECEIVE;

    connection_send_recv_sync(&server->remote, rb);
}

void oto_server_tcp_recv_async(oto_server_tcp_t *server,
                               buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx) {
    struct send_recv_tcp_buffer *rb = allocate(sizeof(struct send_recv_tcp_buffer));

    assert(rb != NULL);

    rb->buffer = buffer;
    rb->host = server;
    rb->bytes_operated = 0;
    rb->cb = cb;
    rb->ctx = ctx;
    rb->op = NETWORK_TCP_OP_RECEIVE;

    connection_send_recv_async(&server->remote, rb, server->master);
}
