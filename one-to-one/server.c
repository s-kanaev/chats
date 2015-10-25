#include "server.h"
#include "memory.h"
#include "endpoint.h"
#include "io-service.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct oto_server_tcp {
    bool connected;
    pthread_mutexattr_t mtx_attr;
    pthread_mutex_t mutex;
    io_service_t *master;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    endpoint_socket_t local;
    endpoint_socket_t remote;
};

struct connection_acceptor {
    oto_server_tcp_t *server;
    oto_connection_cb_t connection_cb;
    void *connection_ctx;
};

struct send_buffer {
    oto_server_tcp_t *server;
    buffer_t *buffer;
    size_t bytes_sent;
    oto_send_cb_t cb;
    void *ctx;
};

static
void oto_tcp_ip4_acceptor(int fd, io_svc_op_t op, void *ctx) {
    struct connection_acceptor *acceptor = ctx;
    oto_server_tcp_t *server = acceptor->server;
    uint32_t addr;
    socklen_t len;

    pthread_mutex_lock(&server->mutex);

    len = sizeof(server->remote_addr);
    int afd = accept(server->local.skt,
                     (struct sockaddr *)(&server->remote_addr),
                     &len);

    assert(len == sizeof(server->remote_addr));
    assert(afd >= 0);

    server->connected = true;
    server->remote.skt = afd;
    server->remote.ep.ep_class = EPC_IP4;
    server->remote.ep.ep.ip4.port = ntohs(server->remote_addr.sin_port);
    addr = ntohl(server->remote_addr.sin_addr.s_addr);
    server->remote.ep.ep.ip4.addr[0] = addr >> 0x18;
    server->remote.ep.ep.ip4.addr[1] = (addr >> 0x10) & 0xff;
    server->remote.ep.ep.ip4.addr[2] = (addr >> 0x08) & 0xff;
    server->remote.ep.ep.ip4.addr[3] = addr & 0xff;

    if (!(*acceptor->connection_cb)(&server->remote.ep,
                                    errno,
                                    acceptor->connection_ctx)) {
        shutdown(server->remote.skt, SHUT_RDWR);
        close(server->remote.skt);
        server->connected = false;
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    deallocate(ctx);
    pthread_mutex_unlock(&server->mutex);
}

static
void oto_send_sync(struct send_buffer *sb) {
    oto_server_tcp_t *server;
    buffer_t *buffer;
    size_t bytes_sent;
    ssize_t bytes_sent_cur;

    assert(sb != NULL);

    server = sb->server;
    buffer = sb->buffer;

    assert(server != NULL);
    assert(buffer != NULL);

    bytes_sent = sb->bytes_sent;

    if (!server->connected) return;

    pthread_mutex_lock(&server->mutex);

    while (bytes_sent < buffer_size(buffer)) {
        bytes_sent_cur = send(server->remote.skt,
                              buffer_data(buffer) + bytes_sent,
                              buffer_size(buffer) - bytes_sent,
                              MSG_NOSIGNAL);
        if (bytes_sent_cur < 0) break;
        bytes_sent += bytes_sent_cur;
    }

    sb->bytes_sent = bytes_sent;

    (*sb->cb)(errno, bytes_sent, buffer, sb->ctx);

    pthread_mutex_unlock(&server->mutex);

    deallocate(sb);
}

static
void oto_send_async(int fd, io_svc_op_t op, void *ctx) {
    struct send_buffer *sb = ctx;
    oto_server_tcp_t *server;
    buffer_t *buffer;
    size_t bytes_sent;
    ssize_t bytes_sent_cur;

    assert(sb != NULL);

    server = sb->server;
    buffer = sb->buffer;

    assert(server != NULL);
    assert(buffer != NULL);

    bytes_sent = sb->bytes_sent;

    if (!server->connected) return;

    pthread_mutex_lock(&server->mutex);

    bytes_sent_cur = send(server->remote.skt,
                          buffer_data(buffer) + bytes_sent,
                          buffer_size(buffer) - bytes_sent,
                          MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytes_sent_cur < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            io_service_post_job(server->master,
                                server->remote.skt,
                                IO_SVC_OP_WRITE,
                                true,
                                oto_send_async,
                                sb);
        else
            (*sb->cb)(errno, bytes_sent, buffer, sb->ctx);
    }
    else {
        bytes_sent += bytes_sent_cur;
        sb->bytes_sent = bytes_sent;
        if (bytes_sent < buffer_size(buffer))
            io_service_post_job(server->master,
                                server->remote.skt,
                                IO_SVC_OP_WRITE,
                                true,
                                oto_send_async,
                                sb);
        else {
            (*sb->cb)(errno, bytes_sent, buffer, sb->ctx);
            deallocate(sb);
        }
    }

    pthread_mutex_unlock(&server->mutex);
}

oto_server_tcp_t *oto_server_tcp_init(io_service_t *svc,
                                      endpoint_class_t epc,
                                      const char *local_addr, unsigned int local_port) {
    char *endptr = NULL, *startptr;
    unsigned long int tmp;
    oto_server_tcp_t *server;
    int socket_family, socket_type = SOCK_STREAM | SOCK_CLOEXEC;

    if (epc >= EPC_MAX || local_addr == NULL) goto fail;

    server = allocate(sizeof(oto_server_tcp_t));

    if (!server) goto fail;

    pthread_mutexattr_init(&server->mtx_attr);
    pthread_mutexattr_settype(&server->mtx_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&server->mutex, &server->mtx_attr);

    server->master = svc;
    server->local.ep.ep_class = epc;
    server->local.ep.ep_type = EPT_TCP;

    memset(&server->local_addr, 0, sizeof(server->local_addr));

    switch (epc) {
    case EPC_IP4:
        server->local.ep.ep.ip4.port = local_port;

        startptr = local_addr;
        tmp = strtoul(startptr, &endptr, 10);
        if (endptr == startptr || *endptr != '.' || tmp >= 256) goto fail;
        server->local.ep.ep.ip4.addr[0] = tmp;

        startptr = endptr + 1;
        tmp = strtoul(startptr, &endptr, 10);
        if (endptr == startptr || *endptr != '.' || tmp >= 256) goto fail;
        server->local.ep.ep.ip4.addr[1] = tmp;

        startptr = endptr + 1;
        tmp = strtoul(startptr, &endptr, 10);
        if (endptr == startptr || *endptr != '.' || tmp >= 256) goto fail;
        server->local.ep.ep.ip4.addr[2] = tmp;

        startptr = endptr + 1;
        tmp = strtoul(startptr, &endptr, 10);
        if (endptr == startptr || *endptr != '\0' || tmp >= 256) goto fail;
        server->local.ep.ep.ip4.addr[3] = tmp;

        socket_family = AF_INET;

        server->local_addr.sin_family = AF_INET;
        server->local_addr.sin_port = htons(server->local.ep.ep.ip4.port);
        server->local_addr.sin_addr.s_addr = htonl(
            (server->local.ep.ep.ip4.addr[0] << 0x18) |
            (server->local.ep.ep.ip4.addr[1] << 0x10) |
            (server->local.ep.ep.ip4.addr[2] << 0x08) |
            (server->local.ep.ep.ip4.addr[3])
        );

        break;

    case EPC_IP6:
        socket_family = AF_INET6;
        goto fail;
        break;

    default:
        goto fail;
        break;
    }

    server->local.skt = socket(socket_family,
                               socket_type | SOCK_CLOEXEC /* | SOCK_NONBLOCK */,
                               0);

    if (server->local.skt < 0) goto fail;

    if (bind(server->local.skt,
             (const struct sockaddr *)(&server->local_addr),
             sizeof(server->local_addr))) goto fail_socket;

    if (listen(server->local.skt, 1)) goto fail_socket;

    server->connected = false;

    return server;

fail_socket:
    close(server->local.skt);

fail:
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

    shutdown(server->remote.skt, SHUT_RDWR);
    close(server->remote.skt);
}

void oto_server_tcp_listen_sync(oto_server_tcp_t *server,
                                oto_connection_cb_t cb, void *ctx) {
    struct connection_acceptor *acceptor;
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (server->connected) {
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    acceptor = allocate(sizeof(struct connection_acceptor));
    assert(acceptor != NULL);

    acceptor->server = server;
    acceptor->connection_cb = cb;
    acceptor->connection_ctx = ctx;

    oto_tcp_ip4_acceptor(server->local.skt, IO_SVC_OP_READ, acceptor);
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_listen_async(oto_server_tcp_t *server,
                                 oto_connection_cb_t cb, void *ctx) {
    struct connection_acceptor *acceptor;
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (server->connected) {
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    acceptor = allocate(sizeof(struct connection_acceptor));
    assert(acceptor != NULL);

    acceptor->server = server;
    acceptor->connection_cb = cb;
    acceptor->connection_ctx = ctx;

    io_service_post_job(server->master,
                        server->local.skt,
                        IO_SVC_OP_READ,
                        true,
                        oto_tcp_ip4_acceptor,
                        acceptor);
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_local_ep(oto_server_tcp_t *server, endpoint_socket_t *ep) {
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    memcpy(ep, &server->local, sizeof(*ep));
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_remote_ep(oto_server_tcp_t *server, endpoint_socket_t *ep) {
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (server->connected)
        memcpy(ep, &server->remote, sizeof(*ep));
    pthread_mutex_unlock(&server->mutex);
}

void oto_server_tcp_send_sync(oto_server_tcp_t *server,
                              buffer_t *buffer,
                              oto_send_cb_t cb, void *ctx) {
    struct send_buffer *sb = allocate(sizeof(struct send_buffer));

    assert(sb != NULL);

    sb->buffer = buffer;
    sb->server = server;
    sb->bytes_sent = 0;
    sb->cb = cb;
    sb->ctx = ctx;
    oto_send_sync(sb);
}

void oto_server_tcp_send_async(oto_server_tcp_t *server,
                               buffer_t *buffer,
                               oto_send_cb_t cb, void *ctx) {
    struct send_buffer *sb = allocate(sizeof(struct send_buffer));

    assert(sb != NULL);

    sb->buffer = buffer;
    sb->server = server;
    sb->bytes_sent = 0;
    sb->cb = cb;
    sb->ctx = ctx;

    io_service_post_job(server->master,
                        server->remote.skt,
                        IO_SVC_OP_WRITE,
                        true,
                        oto_send_async,
                        sb);
}
