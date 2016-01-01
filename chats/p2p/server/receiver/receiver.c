#include "receiver.h"

#include "protocol.h"

#include "memory.h"
#include "thread-pool.h"
#include "io-service.h"
#include "list.h"
#include "hash-map.h"
#include "hash-functions.h"

#include "network.h"
#include "endpoint.h"
#include "one-to-many/server.h"
#include "client/client.h"
#include "connection/connection.h"

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>

#include <arpa/inet.h>

#define MIN_CH2_RCV_BUF_SIZE (sizeof(p2p_header_t) +\
                              (sizeof(p2p_ping) > sizeof(p2p_quit) \
                                ? sizeof(p2p_ping) \
                                : sizeof(p2p_quit)))
#define DEFAULT_CH2_RCV_BUF_SIZE (MIN_CH2_RCV_BUF_SIZE > 1024 \
                                  ? MIN_CH2_RCV_BUF_SIZE : 1024)

enum client_state {
    CS_NONE,
    CS_CONNECTED,
    CS_REFERENCED,
    CS_CHANNEL_SWITCHED
};

typedef struct client_descr {
    const connection_t *connection;
    buffer_t *buffer;
    enum client_state cs;
    char host[P2P_HOST_LENGTH];
    char port[P2P_PORT_LENGTH];
    char nickname[P2P_NICKNAME_LENGTH];
} cd_t;

static struct {
    bool initialized;
    bool running;

    pthread_mutex_t mtx;

    thread_pool_t *tp;
    io_service_t *iosvc;

    buffer_t *ch2_rcv_buffer;

    otm_server_tcp_t *server_ch1;
    client_udp_t *receiver_ch2;

    list_t *clients_list;
    hash_map_t clients_by_nickname;
    hash_map_t clients_by_addr;
} RECEIVER = {
    .initialized = false,
    .running = false
};

/**************************** internal function *****************************/
static
void ch1_reader(endpoint_t ep, int err,
                size_t bytes_operated, size_t has_more_bytes,
                buffer_t *buffer, void *_ctx) {
    cd_t *ctx = _ctx;
    p2p_header_t *header = NULL;
    uint16_t crc_header;
    uint16_t crc_data;

    if (!RECEIVER.running) return;
    assert(RECEIVER.initialized);
    assert(ctx);

    if (err) {
        pthread_mutex_lock(&RECEIVER.mtx);

        otm_server_tcp_disconnect(RECEIVER.server_ch1, ctx->connection);
        list_remove_element(RECEIVER.clients_list, ctx);

        pthread_mutex_unlock(&RECEIVER.mtx);
        return;
    }

    header = buffer_data(ctx->buffer);
    if (!p2p_validate_header(header)) {
        pthread_mutex_lock(&RECEIVER.mtx);

        otm_server_tcp_disconnect(RECEIVER.server_ch1, ctx->connection);
        list_remove_element(RECEIVER.clients_list, ctx);

        pthread_mutex_unlock(&RECEIVER.mtx);
        return;
    }

    switch (ctx->cs) {
        case CS_NONE:
            if (header->cmd != P2P_CMD_CONNECT) {
                pthread_mutex_lock(&RECEIVER.mtx);

                otm_server_tcp_disconnect(RECEIVER.server_ch1, ctx->connection);
                list_remove_element(RECEIVER.clients_list, ctx);

                pthread_mutex_unlock(&RECEIVER.mtx);
                return;
            }

            otm_server_tcp_recv_async();
            otm_server_tcp_recv_more_async();
            buffer_resize(&ctx->buffer, sizeof(p2p_connect_request_t));
            break;
        case CS_CONNECTED:
            break;
        case CS_REFERENCED:
            break;
        case CS_CHANNEL_SWITCHED:
            break;
        default:
            assert(0);
            break;
    }
}

static
bool connection_acceptor(const connection_t *ep, int err, void *ctx) {
    cd_t *client_descr;
    if (!RECEIVER.running) return false;
    assert(RECEIVER.initialized);

    pthread_mutex_lock(&RECEIVER.mtx);

    client_descr = list_append(RECEIVER.clients_list);
    assert(client_descr);

    client_descr->buffer = buffer_init(DEFAULT_CH2_RCV_BUF_SIZE,
                                       buffer_policy_no_shrink);
    assert(client_descr->buffer);
    assert(buffer_resize(&client_descr->buffer, sizeof(p2p_header_t)));

    client_descr->cs = CS_NONE;
    client_descr->connection = ep;
    client_descr->host;

    /* fill in host/port of client_descr */
    assert(
        inet_ntop(
            ep->ep_skt.ep.ep_class == EPC_IP4 ? AF_INET : AF_INET6,
            &ep->ep_skt.ep.addr,
            client_descr->host,
            sizeof(client_descr->host)
        )
    );

    assert(
        0 >= snprintf(
            client_descr->port, sizeof(client_descr->port),
            "%d",
            ep->ep_skt.ep.ep_class == EPC_IP4
             ? ep->ep_skt.ep.ep.ip4.port
             : ep->ep_skt.ep.ep.ip6.port
        )
    );

    /* add recv callback for client_descr */
    otm_server_tcp_recv_async(
        RECEIVER.server_ch1,
        ep,
        client_descr->buffer,
        ch1_reader, client_descr
    );

    pthread_mutex_unlock(&RECEIVER.mtx);
}

void ch2_reader(endpoint_t ep, int err,
                size_t bytes_operated, size_t has_more_bytes,
                buffer_t *buffer, void *ctx) {
    /* TODO */
    if (!RECEIVER.running) return;
    assert(RECEIVER.initialized);
}

/***************************** API functions ********************************/
void receiver_init(thread_pool_t *tp,
                   io_service_t *iosvc,
                   const char *addr, const char *port,
                   int connection_backlog) {
    assert(!RECEIVER.initialized);

    assert(!pthread_mutex_init(&RECEIVER.mtx, NULL));

    RECEIVER.ch2_rcv_buffer = buffer_init(DEFAULT_CH2_RCV_BUF_SIZE,
                                          buffer_policy_no_shrink);
    assert(RECEIVER.ch2_rcv_buffer);

    RECEIVER.server_ch1 = otm_server_tcp_init(iosvc,
                                              addr, port,
                                              connection_backlog,
                                              1);
    assert(RECEIVER.server_ch1);

    RECEIVER.receiver_ch2 = client_udp_init(iosvc, addr, port, 1);
    assert(RECEIVER.receiver_ch2);

    RECEIVER.clients_list = list_init(sizeof(cd_t));
    assert(RECEIVER.clients_list);

    hash_map_init(&RECEIVER.clients_by_addr, pearson_hash);
    hash_map_init(&RECEIVER.clients_by_nickname, pearson_hash);

    RECEIVER.tp = tp;
    RECEIVER.iosvc = iosvc;

    RECEIVER.initialized = true;
    RECEIVER.running = false;
}

void receiver_deinit(void) {
    assert(RECEIVER.initialized);

    pthread_mutex_lock(&RECEIVER.mtx);
    hash_map_deinit(&RECEIVER.clients_by_nickname, false);
    hash_map_deinit(&RECEIVER.clients_by_addr, false);

    list_deinit(RECEIVER.clients_list);

    otm_server_tcp_deinit(RECEIVER.server_ch1);
    client_udp_deinit(RECEIVER.receiver_ch2);

    buffer_deinit(RECEIVER.ch2_rcv_buffer);
    pthread_mutex_unlock(&RECEIVER.mtx);
    pthread_mutex_destroy(&RECEIVER.mtx);

    RECEIVER.initialized = false;
}

void receiver_disconnect_client(void *descr) {
    /* TODO */
}

void receiver_run(void) {
    assert(RECEIVER.initialized);
    RECEIVER.running = true;

    /* do smth else ? */
    otm_server_tcp_listen_async(
        RECEIVER.server_ch1,
        connection_acceptor,
        NULL
    );
    client_udp_recv_async(RECEIVER.receiver_ch2,
                          RECEIVER.ch2_rcv_buffer,
                          ch2_reader,
                          NULL);

    /* TODO set accept, read, write, callbacks */
}
