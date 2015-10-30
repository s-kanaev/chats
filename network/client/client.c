#include "client.h"
#include "memory.h"
#include "endpoint.h"
#include "io-service.h"
#include "connection/connection.h"

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
    endpoint_socket_t remote;
};

struct client_udp {
    int reuse_addr;
    pthread_mutexattr_t mtx_attr;
    pthread_mutex_t mutex;
    io_service_t *master;
    char *local_addr;
    char *local_port;
    endpoint_socket_t local;
};

static
bool client_init_socket(endpoint_type_t ept, endpoint_class_t epc,
                        const char *addr, const char *port, int reuse,
                        endpoint_socket_t *ep_skt) {
    static const int TYPE[] = {
        [EPT_TCP] = SOCK_STREAM,
        [EPT_UDP] = SOCK_DGRAM
    };
    static const int FAMILY[] = {
        [EPC_IP4] = AF_INET,
        [EPC_IP6] = AF_INET6,
        [EPC_NONE] = AF_UNSPEC
    };

    struct addrinfo *addr_info = NULL, *cur_addr, hint;
    int ret, sfd = -1;
    uint32_t ip4addr;
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);

    assert(ept < EPT_MAX && epc <= EPC_MAX);

    ep_skt->skt = -1;

    if (addr == NULL && port == NULL) {
        if (epc == EPC_NONE) epc = EPC_IP4;
        sfd = socket(FAMILY[epc],
                     TYPE[ept] | SOCK_CLOEXEC,
                     0);

        if (sfd < 0) goto fail;

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
                       &reuse,
                       sizeof(reuse))) goto fail;

        ep_skt->skt = sfd;
        ep_skt->ep.ep_type = EPT_UDP;
        assert(0 == getsockname(sfd, (struct sockaddr *)&local_addr, &len));

        ep_skt->ep.ep_class = EPC_IP4;
        ep_skt->ep.ep.ip4.port = ntohs(ep_skt->ep.addr.ip4.sin_port);
        ip4addr = ntohl(ep_skt->ep.addr.ip4.sin_addr.s_addr);
        ep_skt->ep.ep.ip4.addr[0] = ip4addr >> 0x18;
        ep_skt->ep.ep.ip4.addr[1] = (ip4addr >> 0x10) & 0xff;
        ep_skt->ep.ep.ip4.addr[2] = (ip4addr >> 0x08) & 0xff;
        ep_skt->ep.ep.ip4.addr[3] = ip4addr & 0xff;
    } /* if (addr == NULL && port == NULL) */
    else {
        memset(&hint, 0, sizeof(hint));
        hint.ai_family = FAMILY[epc];
        hint.ai_socktype = TYPE[ept];
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
                           &reuse,
                           sizeof(reuse))) goto fail;

            if (!bind(sfd, cur_addr->ai_addr, cur_addr->ai_addrlen)) break;

            shutdown(sfd, SHUT_RDWR);
            close(sfd);
        }

        if (cur_addr == NULL) goto fail;

        ep_skt->ep.ep_type = TYPE[ept];
        ep_skt->skt = sfd;
        memcpy(&ep_skt->ep.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);

        switch (cur_addr->ai_family) {
            case AF_INET:
                ep_skt->ep.ep_class = EPC_IP4;
                ep_skt->ep.ep.ip4.port = ntohs(ep_skt->ep.addr.ip4.sin_port);
                ip4addr = ntohl(ep_skt->ep.addr.ip4.sin_addr.s_addr);
                ep_skt->ep.ep.ip4.addr[0] = ip4addr >> 0x18;
                ep_skt->ep.ep.ip4.addr[1] = (ip4addr >> 0x10) & 0xff;
                ep_skt->ep.ep.ip4.addr[2] = (ip4addr >> 0x08) & 0xff;
                ep_skt->ep.ep.ip4.addr[3] = ip4addr & 0xff;
                break;
            case AF_INET6:
                ep_skt->ep.ep_class = EPC_IP6;
                ep_skt->ep.ep.ip6.port = ntohs(ep_skt->ep.addr.ip6.sin6_port);
                memcpy(ep_skt->ep.ep.ip6.addr,
                       &ep_skt->ep.addr.ip6.sin6_addr,
                       sizeof(ep_skt->ep.ep.ip6.addr));
                break;
        }
    } /* if (addr == NULL && port == NULL) - else */

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
    endpoint_socket_t *ep = &client->remote;
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

    if (connector->connection_cb)
        (*connector->connection_cb)(&client->remote.ep, err, connector->connection_ctx);

    deallocate(connector);
}

/********************** TCP client **********************************/
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
    client->remote.ep.ep_type = client->local.ep.ep_type = EPT_NONE;
    client->remote.ep.ep_class = client->local.ep.ep_class = EPC_NONE;
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
                             tcp_client_connection_cb_t cb, void *ctx) {
    struct addrinfo *addr_info = NULL, *cur_addr, hint;
    endpoint_t *ep;
    int ret;
    uint32_t local_addr;

    if (!client || !(addr && port)) {
        (*cb)(NULL, EADDRNOTAVAIL, ctx);
        return;
    }

    if (client->local.skt >= 0) {
        (*cb)(&client->local.ep, EISCONN, ctx);
        return;
    }

    if (!client_init_socket(EPT_TCP, EPC_NONE,
                            client->local_addr, client->local_port,
                            client->reuse_addr, &client->local)) return;

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

    client->remote.skt = client->local.skt;
    client->remote.ep.ep_type = EPT_TCP;
    memcpy(&client->remote.ep.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);

    switch (cur_addr->ai_family) {
    case AF_INET:
        client->remote.ep.ep_class = EPC_IP4;
        client->remote.ep.ep.ip4.port =
            ntohs(((struct sockaddr_in *)cur_addr->ai_addr)->sin_port);
        local_addr =
            ntohl(((struct sockaddr_in *)cur_addr->ai_addr)->sin_addr.s_addr);
        client->remote.ep.ep.ip4.addr[0] = local_addr >> 0x18;
        client->remote.ep.ep.ip4.addr[1] = (local_addr >> 0x10) & 0xff;
        client->remote.ep.ep.ip4.addr[2] = (local_addr >> 0x08) & 0xff;
        client->remote.ep.ep.ip4.addr[3] = local_addr & 0xff;
        break;

    case AF_INET6:
        client->remote.ep.ep_class = EPC_IP6;
        client->remote.ep.ep.ip6.port =
            ntohs(((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_port);
        memcpy(&client->remote.ep.ep.ip6.addr,
               &((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_addr,
               sizeof(client->remote.ep.ep.ip6.addr));
        break;
    }

    client->connected = true;
    ep = &client->remote.ep;

    if (errno) {
        client_tcp_disconnect(client);
        ep = NULL;
    }

    if (cb) (*cb)(ep, errno, ctx);
    if (addr_info) freeaddrinfo(addr_info);
}

void client_tcp_connect_async(client_tcp_t *client,
                              const char *addr, const char *port,
                              tcp_client_connection_cb_t cb, void *ctx) {
    int flags;
    int errno_old;
    struct addrinfo *addr_info = NULL, *cur_addr, hint;
    int ret;
    struct connector *connector;
    uint32_t local_addr;

    if (!client || !addr || !port) return;

    if (client->local.skt >= 0) {
        (*cb)(&client->local.ep, EISCONN, ctx);
        return;
    }

    if (!client_init_socket(EPT_TCP, EPC_NONE,
                            client->local_addr, client->local_port,
                            client->reuse_addr, &client->local)) return;

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
            memcpy(&client->remote.ep.addr, cur_addr->ai_addr, cur_addr->ai_addrlen);
            switch (cur_addr->ai_addrlen) {
                case sizeof(struct sockaddr_in):
                    client->remote.ep.ep_class = EPC_IP4;
                    client->remote.ep.ep.ip4.port =
                        ntohs(((struct sockaddr_in *)cur_addr->ai_addr)->sin_port);
                    local_addr =
                        ntohl(((struct sockaddr_in *)cur_addr->ai_addr)->sin_addr.s_addr);
                    client->remote.ep.ep.ip4.addr[0] = local_addr >> 0x18;
                    client->remote.ep.ep.ip4.addr[1] = (local_addr >> 0x10) & 0xff;
                    client->remote.ep.ep.ip4.addr[2] = (local_addr >> 0x08) & 0xff;
                    client->remote.ep.ep.ip4.addr[3] = local_addr & 0xff;
                    break;
                case sizeof(struct sockaddr_in6):
                    client->remote.ep.ep_class = EPC_IP6;
                    client->remote.ep.ep.ip6.port =
                        ntohs(((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_port);
                    memcpy(&client->remote.ep.ep.ip6.addr,
                           &((struct sockaddr_in6 *)cur_addr->ai_addr)->sin6_addr,
                           sizeof(client->remote.ep.ep.ip6.addr));
                    break;
                default:
                    assert(0);
                    break;
            }

            io_service_post_job(client->master,
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
    srb_t *srb;
    src_t *src;

    if (!client || !buffer || !buffer_size(buffer))
        return;

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    src = allocate(sizeof(src_t));
    src->cb = cb;
    src->ctx = ctx;

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = send_recv_cb;
    srb->ctx = src;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_RECV;
    srb->iosvc = NULL;
    srb->aux.src = NULL;
    srb->aux.dst = &client->remote;

    srb_operate(srb);
}

void client_tcp_recv_async(client_tcp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;
    src_t *src;

    if (!client || !buffer || !buffer_size(buffer))
        return;

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    src = allocate(sizeof(src_t));
    src->cb = cb;
    src->ctx = ctx;

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = send_recv_cb;
    srb->ctx = src;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_RECV;
    srb->iosvc = client->master;
    srb->aux.src = NULL;
    srb->aux.dst = &client->remote;

    srb_operate(srb);
}

void client_tcp_send_sync(client_tcp_t *client, buffer_t *buffer,
                          network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;
    src_t *src;

    if (!client || !buffer || !buffer_size(buffer))
        return;

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    src = allocate(sizeof(src_t));
    src->cb = cb;
    src->ctx = ctx;

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = send_recv_cb;
    srb->ctx = src;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_SEND;
    srb->iosvc = NULL;
    srb->aux.src = NULL;
    srb->aux.dst = &client->remote;

    srb_operate(srb);
}

void client_tcp_send_async(client_tcp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx) {
    srb_t *srb;
    src_t *src;

    if (!client || !buffer || !buffer_size(buffer))
        return;

    srb = allocate(sizeof(srb_t));
    assert(srb != NULL);

    src = allocate(sizeof(src_t));
    src->cb = cb;
    src->ctx = ctx;

    srb->buffer = buffer;
    srb->bytes_operated = 0;
    srb->cb = send_recv_cb;
    srb->ctx = src;
    srb->operation.type = EPT_TCP;
    srb->operation.op = SRB_OP_SEND;
    srb->iosvc = client->master;
    srb->aux.src = NULL;
    srb->aux.dst = &client->remote;

    srb_operate(srb);
}

/********************** UDP client **********************************/
client_udp_t *client_udp_init(io_service_t *svc,
                              const char *addr, const char *port,
                              int reuse_addr) {
    client_udp_t *client = NULL;
    int socket_family, socket_type = SOCK_STREAM | SOCK_CLOEXEC;

    if (svc == NULL) goto fail;

    client = allocate(sizeof(client_udp_t));

    if (!client) goto fail;

    pthread_mutexattr_init(&client->mtx_attr);
    pthread_mutexattr_settype(&client->mtx_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&client->mutex, &client->mtx_attr);

    client->master = svc;
    client->reuse_addr = reuse_addr;
    client->local_addr = client->local_port = NULL;
    client->local.skt = -1;

    if (addr) client->local_addr = strdup(addr);
    if (port) client->local_port = strdup(port);

    if (!client_init_socket(EPT_UDP, EPC_NONE,
                            client->local_addr, client->local_port,
                            client->reuse_addr, &client->local)) goto fail_socket;

    return client;

fail_socket:
    if (client && client->local.skt >= 0) {
        shutdown(client->local.skt, SHUT_RDWR);
        close(client->local.skt);
    }

fail:
    if (client) {
        if (client->local_addr) free(client->local_addr);
        if (client->local_port) free(client->local_port);
        deallocate(client);
    }
    return NULL;
}

void client_udp_deinit(client_udp_t *client) {
    if (!client) return;

    shutdown(client->local.skt, SHUT_RDWR);
    close(client->local.skt);

    pthread_mutex_destroy(&client->mutex);
    pthread_mutexattr_destroy(&client->mtx_attr);

    if (client->local_addr) free(client->local_addr);
    if (client->local_port) free(client->local_port);
    deallocate(client);
}

void client_udp_local_ep(client_udp_t *client, endpoint_t **ep) {
    if (ep == NULL || client == NULL) return;
    if (*ep == NULL) *ep = allocate(sizeof(endpoint_t));

    memcpy(*ep, &client->local.ep, sizeof(client->local.ep));
}

void client_udp_recv_sync(client_udp_t *client,
                          buffer_t *buffer,
                          const char *addr, const char *port,
                          network_send_recv_cb_t cb, void *ctx) {
//     struct send_recv_buffer *rb = allocate(sizeof(struct send_recv_buffer));
//
//     assert(rb != NULL);
//
//     rb->buffer = buffer;
//     rb->host = client;
//     rb->bytes_operated = 0;
//     rb->cb = cb;
//     rb->ctx = ctx;
//     rb->op = NETWORK_TCP_OP_RECEIVE;
//
//     _udp_send_recv_sync(rb);
}

























