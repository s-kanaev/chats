#include "one-to-many/server.h"
#include <connection/connection.h>
#include "io-service.h"
#include "memory.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
    otm_server_tcp_t *server;
    io_service_t *service;
    buffer_t *buffer;
} context_t;

void data_received(int err, size_t bytes, buffer_t *buffer, void *ctx);
void data_sent(int err, size_t bytes, buffer_t *buffer, void *ctx);

bool connection_accepted(const connection_t *conn, int err, void *ctx) {
    context_t *context = ctx;
    const endpoint_t *ep = &conn->ep_skt.ep;

    fprintf(stdout, "Connection from: %u.%u.%u.%u : %u \n",
            (unsigned)ep->ep.ip4.addr[0],
            (unsigned)ep->ep.ip4.addr[1],
            (unsigned)ep->ep.ip4.addr[2],
            (unsigned)ep->ep.ip4.addr[3],
            (unsigned)ep->ep.ip4.port);

    otm_server_tcp_recv_async(context->server, conn, context->buffer, data_received, (void *)conn);

    otm_server_tcp_listen_async(context->server, connection_accepted, ctx);
    return true;
}

void data_received(int err, size_t bytes, buffer_t *buffer, void *ctx) {
    connection_t *conn = ctx;
    size_t i;
    char *data = buffer_data(buffer);

    fprintf(stdout, "Receival error: %d: %s\n", err, strerror(err));
    if(err) {
        //io_service_stop(context->service, true);
        return;
    }

    fprintf(stdout, "Data received (sending it back - reversed) (size: %llu): %.*s\n",
            buffer_size(buffer), buffer_size(buffer), buffer_data(buffer));

    for (i = 0; i < (buffer_size(buffer) >> 0x01); ++i) {
        char tmp = data[i];
        data[i] = data[buffer_size(buffer) - i - 1];
        data[buffer_size(buffer) - i - 1] = tmp;
    }

    otm_server_tcp_send_async(conn->host, conn, buffer, data_sent, ctx);
}

void data_sent(int err, size_t bytes, buffer_t *buffer, void *ctx) {
    connection_t *conn = ctx;
    fprintf(stdout, "Sending error: %d: %s\n", err, strerror(err));
    //io_service_stop(context->service, true);
    otm_server_tcp_disconnect(conn->host, conn);
}

int main(void) {
    otm_server_tcp_t *server;
    io_service_t *iosvc;
    buffer_t *buffer;
    context_t context;
    endpoint_t ep;

    buffer = buffer_init(10, buffer_policy_no_shrink);
    assert(buffer != NULL);

    iosvc = io_service_init();
    assert(iosvc != NULL);

    ep.ep_type = EPT_TCP;
    ep.ep_class = EPC_IP4;
    ep.ep.ip4.addr[0] = ep.ep.ip4.addr[1] =
    ep.ep.ip4.addr[2] = ep.ep.ip4.addr[3] = 0;
    ep.ep.ip4.port = 12345;

    server = otm_server_tcp_init(iosvc, "0.0.0.0", "12345", 0, 1);
    assert(server != NULL);

    context.buffer = buffer;
    context.server = server;
    context.service = iosvc;

    otm_server_tcp_listen_async(server, connection_accepted, &context);

    io_service_run(iosvc);
    /*io_service_stop(iosvc, true);*/

    otm_server_tcp_deinit(server);
    io_service_deinit(iosvc);
    buffer_deinit(buffer);

    return 0;
}
