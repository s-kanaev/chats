#ifndef _CHATS_CLIENT_H_
# define _CHATS_CLIENT_H_

# include "network.h"
# include "endpoint.h"
# include "io-service.h"
# include "memory.h"

struct client_tcp;
typedef struct client_tcp client_tcp_t;

struct client_udp;
typedef struct client_udp client_udp_t;

client_tcp_t *client_tcp_init(io_service_t *svc,
                              const char *addr, const char *port,
                              int reuse_addr);
void client_tcp_deinit(client_tcp_t *client);
void client_tcp_connect_sync(client_tcp_t *client,
                             const char *addr, const char *port,
                             tcp_client_connection_cb_t cb, void *ctx);
void client_tcp_connect_async(client_tcp_t *client,
                              const char *addr, const char *port,
                              tcp_client_connection_cb_t cb, void *ctx);
void client_tcp_disconnect(client_tcp_t *client);
void client_tcp_local_ep(client_tcp_t *client, endpoint_t **ep);
void client_tcp_remote_ep(client_tcp_t *client, endpoint_t **ep);
void client_tcp_send_sync(client_tcp_t *client, buffer_t *buffer,
                          network_send_recv_cb_t cb, void *ctx);
void client_tcp_send_async(client_tcp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx);
void client_tcp_recv_sync(client_tcp_t *client, buffer_t *buffer,
                          network_send_recv_cb_t cb, void *ctx);
void client_tcp_recv_async(client_tcp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx);

client_udp_t *client_udp_init(io_service_t *svc,
                              const char *addr, const char *port,
                              int reuse_addr);
void client_udp_deinit(client_udp_t *client);
void client_udp_local_ep(client_udp_t *client, endpoint_t **ep);
void client_udp_send_sync(client_udp_t *client, buffer_t *buffer,
                          const char *addr, const char *port,
                          network_send_recv_cb_t cb, void *ctx);
void client_udp_send_async(client_udp_t *client, buffer_t *buffer,
                           const char *addr, const char *port,
                           network_send_recv_cb_t cb, void *ctx);
void client_udp_recv_sync(client_udp_t *client,
                          buffer_t *buffer,
                          network_send_recv_cb_t cb, void *ctx);
void client_udp_recv_async(client_udp_t *client, buffer_t *buffer,
                           network_send_recv_cb_t cb, void *ctx);

#endif /* _CHATS_CLIENT_H_ */