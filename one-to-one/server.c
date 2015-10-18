#include "server.h"
#include "memory.h"
#include "endpoint.h"
#include "io-service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct oto_server_tcp {
    bool connected;
    io_service_t *master;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    endpoint_socket_t local;
    endpoint_socket_t remote;
};

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
        server->local_addr.sin_port = htons(server->local.ep.ip4.port);
        server->local_addr.sin_addr.s_addr = htonl(
            (server->local.ep.ep.ip4.addr[0] << 0x18) |
            (server->local.ep.ep.ip4.addr[1] << 0x10) |
            (server->local.ep.ep.ip4.addr[2] << 0x08) |
            (server->local.ep.ep.ip4.addr[3])
        );

        if (bind(server->local.skt,
                 (const struct sockaddr *)(&server->local_addr),
                 sizeof(server->local_addr))) goto fail;

        if (listen(server->local.skt, 1)) goto fail;

        break;

    case EPC_IP6:
        socket_family = AF_INET6;
        goto fail;
        break;

    default:
        goto fail;
        break;
    }

    server->local.skt = socket(socket_family, socket_type, 0);
    if (server->local.skt < 0) goto fail;

    server->connected = false;

    return server;

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

    deallocate(server);
}

void oto_server_tcp_disconnect(oto_server_tcp_t *server) {
    if (!server) return;

    shutdown(server->remote.skt, SHUT_RDWR);
    close(server->remote.skt);
}

void oto_server_tcp_listen_sync(oto_server_tcp_t *server,
                                oto_connection_cb_t cb, void *ctx) {
    if (!server) return;
    if (server->connected) return;

    accept();
}

