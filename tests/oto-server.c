#include "one-to-one/server.h"
#include "io-service.h"
#include "memory.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
    oto_server_tcp_t *server;
    io_service_t *service;
    buffer_t *buffer;
} context_t;

void data_received(int err, size_t bytes, buffer_t *buffer, void *ctx);
void data_sent(int err, size_t bytes, buffer_t *buffer, void *ctx);

bool connection_accepted(endpoint_t *ep, int err, void *ctx) {
    context_t *context = ctx;

    fprintf(stdout, "Connection from: %u.%u.%u.%u : %u \n",
            (unsigned)ep->ep.ip4.addr[0],
            (unsigned)ep->ep.ip4.addr[1],
            (unsigned)ep->ep.ip4.addr[2],
            (unsigned)ep->ep.ip4.addr[3],
            (unsigned)ep->ep.ip4.port);

    oto_server_tcp_recv_async(context->server, context->buffer, data_received, ctx);

    return true;
}

void data_received(int err, size_t bytes, buffer_t *buffer, void *ctx) {
    context_t *context = ctx;

    fprintf(stdout, "Receival error: %d: %s\n", err, strerror(err));
    if(err) {
        io_service_stop(context->service, true);
        return;
    }

    fprintf(stdout, "Data received (sending it back) (size: %llu): %.*s\n",
            buffer_size(buffer), buffer_size(buffer), buffer_data(buffer));

    oto_server_tcp_send_async(context->server, buffer, data_sent, ctx);
}

void data_sent(int err, size_t bytes, buffer_t *buffer, void *ctx) {
    context_t *context = ctx;
    fprintf(stdout, "Sending error: %d: %s\n", err, strerror(err));
    io_service_stop(context->service, true);
}

int main(int argc, char *argv[]) {
    oto_server_tcp_t *server;
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

    server = oto_server_tcp_init(iosvc, &ep, 1);
    assert(server != NULL);

    context.buffer = buffer;
    context.server = server;
    context.service = iosvc;

    oto_server_tcp_listen_async(server, connection_accepted, &context);

    io_service_run(iosvc);
    /*io_service_stop(iosvc, true);*/

    oto_server_tcp_deinit(server);
    io_service_deinit(iosvc);
    buffer_deinit(buffer);

    return 0;
}