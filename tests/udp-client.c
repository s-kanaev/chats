#include "client/client.h"
#include "io-service.h"
#include "memory.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    client_udp_t *client;
    io_service_t *service;
    buffer_t *buffer;
} context_t;

void data_received(endpoint_t ep, int err, size_t bytes, size_t more_bytes, buffer_t *buffer, void *ctx);
void data_sent(endpoint_t ep, int err, size_t bytes, size_t more_bytes, buffer_t *buffer, void *ctx);

void data_received(endpoint_t ep, int err, size_t bytes, size_t more_bytes, buffer_t *buffer, void *ctx) {
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

void data_sent(endpoint_t ep, int err, size_t bytes, size_t more_bytes, buffer_t *buffer, void *ctx) {
    context_t *context = ctx;
    fprintf(stdout, "Sending error (waiting for echo): %d: %s\n", err, strerror(err));

    assert(err == 0);

    client_udp_recv_async(context->client, buffer, data_received, ctx);
}

int main(int argc, char *argv[]) {
    client_udp_t *client;
    io_service_t *iosvc;
    buffer_t *buffer;
    context_t context;
    endpoint_t ep;

    if (argc < 3) {
        fprintf(stdout, "usage: %s <src-port> <dst-port>\n", argv[0]);
        exit(0);
    }

    buffer = buffer_init(10, buffer_policy_no_shrink);
    assert(buffer != NULL);

    iosvc = io_service_init();
    assert(iosvc != NULL);

    ep.ep_type = EPT_TCP;
    ep.ep_class = EPC_IP4;
    ep.ep.ip4.addr[0] = ep.ep.ip4.addr[1] =
    ep.ep.ip4.addr[2] = ep.ep.ip4.addr[3] = 0;
    ep.ep.ip4.port = 12345;

    client = client_udp_init(iosvc, NULL /*"127.0.0.1"*/, argv[1] /*"54321"*/, 1);
    assert(client != NULL);

    context.buffer = buffer;
    context.client = client;
    context.service = iosvc;

    buffer_resize(&buffer, 10);
    memcpy(buffer_data(buffer), "1234567890", 10);

    //client_udp_connect_sync(client, "127.0.0.1", "12345", connected, &context);

    //oto_server_tcp_listen_async(server, connection_accepted, &context);

    client_udp_send_async(context.client, context.buffer, "127.0.0.1", argv[2], data_sent, &context);
    io_service_run(iosvc);
    /*io_service_stop(iosvc, true);*/

    client_udp_deinit(client);
    io_service_deinit(iosvc);
    buffer_deinit(buffer);

    return 0;
}