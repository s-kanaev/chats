#include "server.h"
#include "memory.h"
#include "list.h"
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

struct otm_server_tcp {
    int reuse_addr;
    pthread_mutexattr_t mtx_attr;
    pthread_mutex_t mutex;
    io_service_t *master;
    endpoint_socket_t local;
    list_t *remotes_list;
};

static
void close_connection(const connection_t *connection) {
    shutdown(connection->ep_skt.skt, SHUT_RDWR);
    close(connection->ep_skt.skt);
}

static
void tcp_acceptor(int fd, io_svc_op_t op, void *ctx) {
    struct connection_acceptor *acceptor = ctx;
    otm_server_tcp_t *server = acceptor->host;
    socklen_t len;
    struct sockaddr *dest_addr;
    int afd;
    connection_t *connection;

    pthread_mutex_lock(&server->mutex);

    connection = list_append(server->remotes_list);
    assert(connection);

    connection->host = server;
    dest_addr = (struct sockaddr *)&connection->ep_skt.ep.addr;
    len = sizeof(connection->ep_skt.ep.addr);

    errno = 0;
    afd = accept(server->local.skt, dest_addr, &len);
    assert(afd >= 0);

    connection->ep_skt.skt = afd;
    connection->ep_skt.ep.ep_type = EPT_TCP;
    translate_endpoint(&connection->ep_skt.ep);

    if (!(*acceptor->connection_cb)(connection,
                                    errno,
                                    acceptor->connection_ctx)) {
        close_connection(connection);
        list_remove_element(server->remotes_list, connection);
    }

    deallocate(ctx);
    pthread_mutex_unlock(&server->mutex);
}

otm_server_tcp_t *otm_server_tcp_init(io_service_t *svc,
                                      const char *addr, const char *port,
                                      int connection_backlog,
                                      int reuse_addr) {
    otm_server_tcp_t *server = NULL;
    int ret;
    int sfd;
    struct addrinfo *addr_info = NULL, *cur_addr;
    struct addrinfo hint;
    int socket_family;
    uint32_t addr_ip4;

    if (svc == NULL || addr == NULL || port == NULL) goto fail;
    if (connection_backlog <= 0) connection_backlog = DEFAULT_CONNECTION_BACKLOG;

    server = allocate(sizeof(otm_server_tcp_t));

    if (!server) goto fail;

    pthread_mutexattr_init(&server->mtx_attr);
    pthread_mutexattr_settype(&server->mtx_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&server->mutex, &server->mtx_attr);

    server->master = svc;
    server->reuse_addr = reuse_addr;

    server->remotes_list = list_init(sizeof(connection_t));

    if (server->remotes_list == NULL) goto fail;

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
    translate_endpoint(&server->local.ep);

    if (listen(server->local.skt, connection_backlog)) goto fail_socket;

    freeaddrinfo(addr_info);
    return server;

fail_socket:
    shutdown(server->local.skt, SHUT_RDWR);
    close(server->local.skt);

fail:
    if (addr_info) freeaddrinfo(addr_info);
    if (server) {
        if (server->remotes_list) list_deinit(server->remotes_list);
        deallocate(server);
    }
    return NULL;
}

void otm_server_tcp_deinit(otm_server_tcp_t *server) {
    connection_t *connection;
    list_t *remotes_list;

    if (!server) return;

    pthread_mutex_lock(&server->mutex);

    remotes_list = server->remotes_list;
    for (connection = list_first_element(remotes_list);
         connection;
         connection = list_remove_next(remotes_list, connection))
        close_connection(connection);

    shutdown(server->local.skt, SHUT_RDWR);
    close(server->local.skt);

    pthread_mutex_unlock(&server->mutex);
    pthread_mutex_destroy(&server->mutex);
    pthread_mutexattr_destroy(&server->mtx_attr);

    deallocate(server);
}

void otm_server_tcp_disconnect(otm_server_tcp_t *server,
                               const connection_t *connection) {
    if (!server || !connection || connection->host != server) return;

    pthread_mutex_lock(&server->mutex);
    close_connection(connection);
    list_remove_element(server->remotes_list, (void *)connection);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_listen_sync(otm_server_tcp_t *server,
                                tcp_connection_cb_t cb, void *ctx) {
    struct connection_acceptor *acceptor;
    if (!server) return;

    pthread_mutex_lock(&server->mutex);

    acceptor = allocate(sizeof(struct connection_acceptor));
    assert(acceptor != NULL);

    acceptor->host = server;
    acceptor->connection_cb = cb;
    acceptor->connection_ctx = ctx;

    tcp_acceptor(server->local.skt, IO_SVC_OP_READ, acceptor);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_listen_async(otm_server_tcp_t *server,
                                 tcp_connection_cb_t cb, void *ctx) {
    struct connection_acceptor *acceptor;
    if (!server) return;

    pthread_mutex_lock(&server->mutex);

    acceptor = allocate(sizeof(struct connection_acceptor));
    assert(acceptor != NULL);

    acceptor->host = server;
    acceptor->connection_cb = cb;
    acceptor->connection_ctx = ctx;

    io_service_post_job(server->master,
                        server->local.skt,
                        IO_SVC_OP_READ,
                        true,
                        tcp_acceptor,
                        acceptor);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_local_ep(otm_server_tcp_t *server, endpoint_socket_t *ep) {
    if (!server || !ep) return;

    pthread_mutex_lock(&server->mutex);
    memcpy(ep, &server->local, sizeof(*ep));
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_send_sync(otm_server_tcp_t *server,
                              const connection_t *connection,
                              buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;

    if (!server || !connection || !buffer ||
        !buffer_size(buffer) || connection->host != server)
        return;

    pthread_mutex_lock(&server->mutex);

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = cb;
    srb->ctx = ctx;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_SEND;
    srb->iosvc = NULL;
    srb->aux.src.skt = -1;
    srb->aux.dst = connection->ep_skt;

    srb_operate(srb);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_send_async(otm_server_tcp_t *server,
                               const connection_t *connection,
                               buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;

    if (!server || !connection || !buffer ||
        !buffer_size(buffer) || connection->host != server)
        return;

    pthread_mutex_lock(&server->mutex);

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = cb;
    srb->ctx = ctx;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_SEND;
    srb->iosvc = server->master;
    srb->aux.src.skt = -1;
    srb->aux.dst = connection->ep_skt;

    srb_operate(srb);

    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_recv_sync(otm_server_tcp_t *server,
                              const connection_t *connection,
                              buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;

    if (!server || !connection || !buffer ||
        !buffer_size(buffer) || connection->host != server)
        return;

    pthread_mutex_lock(&server->mutex);

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = cb;
    srb->ctx = ctx;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_RECV;
    srb->iosvc = NULL;
    srb->aux.src = connection->ep_skt;
    srb->aux.dst.skt = -1;

    srb_operate(srb);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_recv_async(otm_server_tcp_t *server,
                               const connection_t *connection,
                               buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;

    if (!server || !connection || !buffer ||
        !buffer_size(buffer) || connection->host != server)
        return;

    pthread_mutex_lock(&server->mutex);

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = cb;
    srb->ctx = ctx;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_RECV;
    srb->iosvc = server->master;
    srb->aux.src = connection->ep_skt;
    srb->aux.dst.skt = -1;

    srb_operate(srb);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_recv_more_sync(otm_server_tcp_t *server,
                                   const connection_t *connection,
                                   buffer_t **buffer, size_t how_much,
                                   network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;

    if (!server || !connection || !buffer ||
        connection->host != server)
        return;

    pthread_mutex_lock(&server->mutex);

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    srb->bytes_operated = buffer_size(*buffer);
    assert(buffer_resize(buffer, srb->bytes_operated + how_much));
    srb->buffer = *buffer;

    srb->cb = cb;
    srb->ctx = ctx;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_RECV;
    srb->iosvc = NULL;
    srb->aux.src = connection->ep_skt;
    srb->aux.dst.skt = -1;

    srb_operate(srb);
    pthread_mutex_unlock(&server->mutex);
}

void otm_server_tcp_recv_more_async(otm_server_tcp_t *server,
                                    const connection_t *connection,
                                    buffer_t **buffer, size_t how_much,
                                    network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;

    if (!server || !connection || !buffer ||
        connection->host != server)
        return;

    pthread_mutex_lock(&server->mutex);

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    srb->bytes_operated = buffer_size(*buffer);
    assert(buffer_resize(buffer, srb->bytes_operated + how_much));
    srb->buffer = *buffer;

    srb->cb = cb;
    srb->ctx = ctx;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_RECV;
    srb->iosvc = server->master;
    srb->aux.src = connection->ep_skt;
    srb->aux.dst.skt = -1;

    srb_operate(srb);
    pthread_mutex_unlock(&server->mutex);
}
