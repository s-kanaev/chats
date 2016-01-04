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

#define UNUSED __attribute__ ((unused))

#define MIN_CH2_RCV_BUF_SIZE (sizeof(p2p_header_t) + \
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
    buffer_t *ch1_rcv_buffer;
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

    pthread_attr_t ch2_thread_attr;
    pthread_t ch2_thread_id;

    struct timeval ch1_rcv_timeout;

    otm_server_tcp_t *server_ch1;
    client_udp_t *receiver_ch2;

    list_t *clients_list;   /** list of \c cd_t */
    hash_map_t clients_by_nickname; /** map nickname to \c clients_list element */
} RECEIVER = {
    .initialized = false,
    .running = false,
    .ch1_rcv_timeout = {
        .tv_sec = 3,
        .tv_usec = 0
    }
};

/**************************** internal function *****************************/
/*************** channel 1 functions ******************/
/**
 * ctx->cs = CS_NONE
 * \param ctx client description
 * \return \c true until error
 * \post buffer contains header with connection request
 *       with \c '\0' put at the end of nickname to prevent
 *       moving out of allocated memory block.
 *       nickname is filled to ctx
 */
static
bool ch1_talker_init(cd_t *ctx) {
    network_result_t net_ret;
    p2p_header_t *header;
    p2p_connect_request_t *cr;
    uint16_t crc_header;
    uint16_t crc_data;

    assert(buffer_resize(&ctx->ch1_rcv_buffer, sizeof(p2p_header_t)));

    net_ret = otm_server_tcp_recv_sync(
        RECEIVER.server_ch1,
        ctx->connection,
        ctx->ch1_rcv_buffer,
        0
    );

    if (net_ret.err) return false;

    header = buffer_data(ctx->ch1_rcv_buffer);
    if (p2p_validate_header(header)) return false;

    if (header->cmd != P2P_CMD_CONNECT) return false;

    assert(buffer_resize(
        &ctx->ch1_rcv_buffer, sizeof(p2p_header_t) + sizeof(p2p_connect_request_t)
    ));

    net_ret = otm_server_tcp_recv_sync(
        RECEIVER.server_ch1,
        ctx->connection,
        ctx->ch1_rcv_buffer,
        sizeof(p2p_header_t)
    );

    if (net_ret.err) return false;

    header = buffer_data(ctx->ch1_rcv_buffer);
    if (p2p_check_data_crc(header)) return false;

    cr = (const p2p_connect_request_t *)(header + 1);
    cr->nickname[sizeof(cr->nickname) - 1] = '\0';
    memcpy(ctx->nickname, cr->nickname, sizeof(ctx->nickname));

    return true;
}

/**
 * ctx->cs = CS_CONNECTED
 * \param ctx client description
 * \return \c true untill error
 * \post buffer containing header and a reference
 *       is sent to client
 */
static
bool ch1_talker_send_reference(cd_t *ctx) {
    network_result_t net_ret;
    char *rcv_buf_data;
    p2p_header_t *header;
    p2p_reference_t *reference;
    p2p_reference_entry_t *ref_entry;
    avl_tree_node_t *client_node;
    uint16_t crc_header;
    uint16_t crc_data;
    size_t ref_size;

    pthread_mutex_lock(&RECEIVER.mtx);

    ref_size = hash_map_count(&RECEIVER.clients_by_nickname)

    assert(buffer_resize(
        &ctx->ch1_rcv_buffer,
        sizeof(p2p_header_t) + sizeof(p2p_reference_t)
        + sizeof(p2p_reference_entry_t) * ref_size
    ));

    header = buffer_data(ctx->ch1_rcv_buffer);

    memcpy(header->signature, P2P_SIGNATURE, sizeof(header->signature));
    header->cmd = P2P_CMD_REFERENCE;
    header->length = sizeof(p2p_reference_t) + sizeof(p2p_reference_entry_t) * ref_size;

    reference = (p2p_reference_t *)(header + 1);
    reference->clients_count = ref_size;

    ref_entry = (p2p_reference_entry_t)(reference + 1);
    client_node = avl_tree_min(&RECEIVER.clients_by_nickname.tree.root);
    for (; ref_size; --ref_size, ++ref_entry, client_node = avl_tree_next(client_node)) {
        cd_t *other_client = client_node->data;
        memcpy(ref_entry->nickname, other_client->nickname, sizeof(ref_entry->nickname));
        memcpy(ref_entry->port, other_client->port, sizeof(ref_entry->port));
        memcpy(ref_entry->ip, other_client->host, sizeof(ref_entry->ip));
    }

    pthread_mutex_unlock(&RECEIVER.mtx);

    p2p_put_header_crc(header);
    p2p_put_data_crc(header);

    net_ret = otm_server_tcp_send_sync(
        RECEIVER.server_ch1, ctx->connection,
        ctx->ch1_rcv_buffer, 0
    );

    if (net_ret.err) return false;

    return true;
}

/**
 * ctx->cs = CS_REFERENCED
 * \param ctx client description
 * \return \c true until error
 * \post buffer contains valid channel switching request
 */
static
bool ch1_talker_channel_switcher(cd_t *ctx) {
    network_result_t net_ret;
    p2p_header_t *header;
    uint16_t crc_header;
#if 0
    uint16_t crc_data;
#endif

    assert(buffer_resize(&ctx->ch1_rcv_buffer, sizeof(p2p_header_t)));

    net_ret = otm_server_tcp_recv_sync(
        RECEIVER.server_ch1,
        ctx->connection,
        ctx->ch1_rcv_buffer,
        0
    );

    if (net_ret.err) return false;

    header = buffer_data(ctx->ch1_rcv_buffer);
    if (p2p_validate_header(header)) return false;

    if (header->cmd != P2P_CMD_CHANNEL_SWITCH) return false;
#if 0
    assert(buffer_resize(
        &ctx->ch1_rcv_buffer, sizeof(p2p_header_t) + sizeof(p2p_channel_switch_t)
    ));

    net_ret = otm_server_tcp_recv_sync(
        RECEIVER.server_ch1,
        ctx->connection,
        ctx->ch1_rcv_buffer,
        sizeof(p2p_header_t)
    );

    if (net_ret.err) return false;

    header = buffer_data(ctx->ch1_rcv_buffer);
    if (p2p_check_data_crc(header)) return false;
#endif

    return true;
}

/**
 * ctx->cs = CS_CHANNEL_SWITCHED
 * \param ctx client description
 * \param self_node reference to the client in by nockname hash map
 * \return \c true until error
 * \post buffer containing reference add command
 */
static
bool ch1_talker_notify_others(cd_t *ctx, avl_tree_node_t *self_node) {
    avl_tree_node_t *client_node;
    network_result_t net_ret;
    p2p_header_t *header;
    p2p_reference_add_t *ref_entry;

    assert(buffer_resize(
        &ctx->ch1_rcv_buffer,
        sizeof(p2p_header_t) + sizeof(p2p_reference_add_t)
    ));

    header = buffer_data(ctx->ch1_rcv_buffer);
    ref_entry = (p2p_reference_add_t *)(header + 1);

    memcpy(header->signature, P2P_SIGNATURE, sizeof(header->signature));
    header->cmd = P2P_CMD_REFERENCE_ADD;
    header->length = sizeof(p2p_reference_add_t);

    memcpy(ref_entry->entry.nickname, ctx->nickname, sizeof(ref_entry->entry.nickname));
    memcpy(ref_entry->entry.port, ctx->port, sizeof(ref_entry->entry.port));
    memcpy(ref_entry->entry.ip, ctx->host, sizeof(ref_entry->entry.ip));

    p2p_put_header_crc(header);
    p2p_put_data_crc(header);

    pthread_mutex_lock(&RECEIVER.mtx);

    client_node = avl_tree_min(RECEIVER.clients_by_nickname.tree.root);
    for (; client_node && client_node != self_node; client_node = avl_tree_next(client_node)) {
        cd_t *other_client = client_node->data;

        net_ret = client_udp_send_sync(
            RECEIVER.receiver_ch2,
            ctx->ch1_rcv_buffer, 0,
            other_client->host, other_client->port
        );
    }

    client_node = avl_tree_next(client_node);
    for (; client_node; client_node = avl_tree_next(client_node)) {
        cd_t *other_client = client_node->data;

        net_ret = client_udp_send_sync(
            RECEIVER.receiver_ch2,
            ctx->ch1_rcv_buffer, 0,
            other_client->host, other_client->port
        );
    }

    pthread_mutex_unlock(&RECEIVER.mtx);
}

static
void ch1_talker_dismiss(cd_t *ctx, bool lock) {
    if (lock) pthread_mutex_lock(&RECEIVER.mtx);

    otm_server_tcp_disconnect(RECEIVER.server_ch1, ctx->connection);
    buffer_deinit(ctx->ch1_rcv_buffer);
    list_remove_element(RECEIVER.clients_list, ctx);

    if (lock) pthread_mutex_unlock(&RECEIVER.mtx);
}

static
void ch1_talker(void *_ctx) {
    cd_t *ctx = _ctx;
    bool ret;
    char *rcv_buf_data;
    avl_tree_node_t *client_by_nickname;
    long long int nickname_hash;

    if (!RECEIVER.running) return;
    assert(RECEIVER.initialized);
    assert(ctx);

    /* ctx->cs = CS_NONE */
    ret = ch1_talker_init(ctx);
    if (!ret) {
        ch1_talker_dismiss(ctx, true);
        return;
    }

    /* check if the nickname is already used */
    rcv_buf_data = buffer_data(ctx->ch1_rcv_buffer);
    rcv_buf_data += sizeof(p2p_header_t);
    nickname_hash = pearson_hash(rcv_buf_data, strlen(rcv_buf_data));

    pthread_mutex_lock(&RECEIVER.mtx);
    client_by_nickname = hash_map_get_by_hash(
        &RECEIVER.clients_by_nickname, nickname_hash
    );
    if (client_by_nickname) {
        ch1_talker_dismiss(ctx, false);
        pthread_mutex_unlock(&RECEIVER.mtx);
        return;
    }
    pthread_mutex_unlock(&RECEIVER.mtx);

    /* transit to connected state */
    ctx->cs = CS_CONNECTED;

    /* send reference to client */
    if (!ch1_talker_send_reference(ctx)) {
        ch1_talker_dismiss(ctx, true);
        return;
    }

    /* transit to referenced state */
    ctx->cs = CS_REFERENCED;

    /* wait for channel switching request */
    if (!ch1_talker_channel_switcher(ctx)) {
        ch1_talker_dismiss(ctx, true);
        return;
    }

    /* transit to channel switched state */
    ctx->cs = CS_CHANNEL_SWITCHED;

    /* add the client to ref list */
    pthread_mutex_lock(&RECEIVER.mtx);
    client_by_nickname = hash_map_insert_by_hash(
        &RECEIVER.clients_by_nickname,
        nickname_hash,
        ctx
    );
    pthread_mutex_unlock(&RECEIVER.mtx);

    /* send reference changes to others */
    if (!ch1_talker_notify_others(ctx, client_by_nickname)) return;

    return;
}

static
bool ch1_connection_acceptor(const connection_t *ep, int err, void *ctx) {
    cd_t *client_descr;
    if (!RECEIVER.running) return false;
    assert(RECEIVER.initialized);

    pthread_mutex_lock(&RECEIVER.mtx);

    client_descr = list_append(RECEIVER.clients_list);
    assert(client_descr);

    client_descr->ch1_rcv_buffer = buffer_init(DEFAULT_CH2_RCV_BUF_SIZE,
                                       buffer_policy_no_shrink);
    assert(client_descr->ch1_rcv_buffer);
    assert(buffer_resize(&client_descr->ch1_rcv_buffer, sizeof(p2p_header_t)));

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

    setsockopt(ep->ep_skt.skt, SOL_SOCKET, SO_RCVTIMEO,
               &RECEIVER.ch1_rcv_timeout, sizeof(RECEIVER.ch1_rcv_timeout));

    /* talk to the client in another thread */
    thread_pool_post_job(RECEIVER.tp, ch1_talker, client_descr);

    /* let's accept one more connection */
    otm_server_tcp_listen_async(
        RECEIVER.server_ch1,
        ch1_connection_acceptor,
        NULL
    );

    pthread_mutex_unlock(&RECEIVER.mtx);

    return true;
}

/*************** channel 2 functions ******************/
/* permanent channel-2 data awaiting thread */
static
void ch2_reader(void *_ctx UNUSED) {
    buffer_t *buffer;
    network_result_t net_ret;

    if (!RECEIVER.running) return;
    assert(RECEIVER.initialized);

    buffer = buffer_init(sizeof(p2p_header_t));
    assert(buffer);

    assert(0);

    pthread_mutex_lock(&RECEIVER.mtx);
    while (RECEIVER.running) {
        pthread_mutex_unlock(&RECEIVER.mtx);

        assert(buffer_resize(
            &buffer,
            sizeof(p2p_header_t)
        ));

        net_ret = client_udp_recv_sync(
            RECEIVER.receiver_ch2,
            buffer, 0
        );

        if (net_ret.err) {
            pthread_mutex_lock(&RECEIVER.mtx);
            continue;
        }

        /* TODO */

        pthread_mutex_lock(&RECEIVER.mtx);
    }
    pthread_mutex_unlock(&RECEIVER.mtx);

    buffer_deinit(buffer);
}

/***************************** API functions ********************************/
void receiver_init(thread_pool_t *tp,
                   io_service_t *iosvc,
                   const char *addr, const char *port,
                   int connection_backlog) {
    assert(!RECEIVER.initialized);

    assert(!pthread_mutex_init(&RECEIVER.mtx, NULL));

    RECEIVER.server_ch1 = otm_server_tcp_init(iosvc,
                                              addr, port,
                                              connection_backlog,
                                              1);
    assert(RECEIVER.server_ch1);

    RECEIVER.receiver_ch2 = client_udp_init(iosvc, addr, port, 1);
    assert(RECEIVER.receiver_ch2);

    RECEIVER.clients_list = list_init(sizeof(cd_t));
    assert(RECEIVER.clients_list);

    hash_map_init(&RECEIVER.clients_by_nickname, pearson_hash);

    RECEIVER.tp = tp;
    RECEIVER.iosvc = iosvc;

    RECEIVER.initialized = true;
    RECEIVER.running = false;
}

void receiver_deinit(void) {
    assert(RECEIVER.initialized);

    if (RECEIVER.running) {
        /* TODO interrupt ch2_reader thread */
    }

    pthread_mutex_lock(&RECEIVER.mtx);
    hash_map_deinit(&RECEIVER.clients_by_nickname, false);

    list_deinit(RECEIVER.clients_list);

    otm_server_tcp_deinit(RECEIVER.server_ch1);
    client_udp_deinit(RECEIVER.receiver_ch2);

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
        ch1_connection_acceptor,
        NULL
    );

    /* TODO post ch2_reader on another thread */
    thread_pool_post_job(RECEIVER.tp, ch2_reader, NULL);

    /* TODO set accept, read, write, callbacks */
}
