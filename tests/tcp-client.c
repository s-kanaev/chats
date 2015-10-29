#include "client/client.h"
#include "io-service.h"
#include "memory.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
    client_tcp_t *client;
    io_service_t *service;
    buffer_t *buffer;
} context_t;

void data_received(int err, size_t bytes, buffer_t *buffer, void *ctx);
void data_sent(int err, size_t bytes, buffer_t *buffer, void *ctx);

void data_received(int err, size_t bytes, buffer_t *buffer, void *ctx) {
    context_t *context = ctx;

    fprintf(stdout, "Receival error: %d: %s\n", err, strerror(err));
    if(err) {
        io_service_stop(context->service, true);
        return;
    }

    fprintf(stdout, "Data received (size: %llu): %.*s\n",
            buffer_size(buffer), buffer_size(buffer), buffer_data(buffer));

    io_service_stop(context->service, true);
}

void data_sent(int err, size_t bytes, buffer_t *buffer, void *ctx) {
    context_t *context = ctx;
    fprintf(stdout, "Sending error (waiting for echo): %d: %s\n", err, strerror(err));

    assert(err == 0);

    client_tcp_recv_async(context->client, buffer, data_received, ctx);
}

bool connected(const endpoint_t *ep, int err,
               void *ctx) {
    context_t *context = ctx;

    fprintf(stdout, "Connection error (Sending some data): %d: %s\n", err, strerror(err));

    if (err || ep == NULL) {
        io_service_stop(context->service, false);
        return;
    }

    fprintf(stdout, "Connected to: %u.%u.%u.%u : %u \n",
            (unsigned)ep->ep.ip4.addr[0],
            (unsigned)ep->ep.ip4.addr[1],
            (unsigned)ep->ep.ip4.addr[2],
            (unsigned)ep->ep.ip4.addr[3],
            (unsigned)ep->ep.ip4.port);

    client_tcp_send_async(context->client, context->buffer, data_sent, ctx);

    return true;
}

int main(void) {
    client_tcp_t *client;
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

    client = client_tcp_init(iosvc, NULL /*"127.0.0.1"*/, NULL /*"54321"*/, 1);
    assert(client != NULL);

    context.buffer = buffer;
    context.client = client;
    context.service = iosvc;

    buffer_resize(&buffer, 10);
    memcpy(buffer_data(buffer), "1234567890", 10);

    client_tcp_connect_sync(client, "127.0.0.1", "12345", connected, &context);

    //oto_server_tcp_listen_async(server, connection_accepted, &context);

    io_service_run(iosvc);
    /*io_service_stop(iosvc, true);*/

    client_tcp_deinit(client);
    io_service_deinit(iosvc);
    buffer_deinit(buffer);

    return 0;
}