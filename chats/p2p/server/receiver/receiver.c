#include "receiver.h"
#include "thread-pool.h"
#include "io-service.h"

#include "network.h"
#include "endpoint.h"
#include "one-to-many/server.h"
#include "client/client.h"
#include "connection/connection.h"

static struct {
    otm_server_tcp_t *server_ch1;
    client_udp_t *receiver_ch2;
} RECEIVER;

void receiver_init(thread_pool_t *tp,
                   io_service_t *iosvc,
                   const char *addr, const char *port,
                   int connection_backlog) {
    /* TODO */
}

void receiver_deinit(void) {
    /* TODO */
}

void receiver_disconnect_client(void *descr) {
    /* TODO */
}

void receiver_run(void) {
    /* TODO */
}
