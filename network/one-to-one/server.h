#ifndef _CHATS_ONE_TO_ONE_SERVER_H_
# define _CHATS_ONE_TO_ONE_SERVER_H_

# include "network.h"
# include "endpoint.h"
# include "io-service.h"
# include "memory.h"

/* oto stands for one-to-one */

struct oto_server_tcp;
typedef struct oto_server_tcp oto_server_tcp_t;

oto_server_tcp_t *oto_server_tcp_init(io_service_t *svc,
                                      const char *addr, const char *port,
                                      int reuse_addr);
void oto_server_tcp_deinit(oto_server_tcp_t *server);
void oto_server_tcp_listen_sync(oto_server_tcp_t *server,
                                tcp_connection_cb_t cb, void *ctx);
void oto_server_tcp_listen_async(oto_server_tcp_t *server,
                                 tcp_connection_cb_t cb, void *ctx);
void oto_server_tcp_disconnect(oto_server_tcp_t *server);
void oto_server_tcp_send_sync(oto_server_tcp_t *server, buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_send_async(oto_server_tcp_t *server, buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_recv_sync(oto_server_tcp_t *server, buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_recv_async(oto_server_tcp_t *server, buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_recv_more_async(oto_server_tcp_t *server,
                                    buffer_t **buffer, size_t how_much,
                                    network_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_recv_more_sync(oto_server_tcp_t *server,
                                   buffer_t **buffer, size_t how_much,
                                   network_send_recv_cb_t cb, void *ctx);

void oto_server_tcp_local_ep(oto_server_tcp_t *server, endpoint_socket_t *ep);
void oto_server_tcp_remote_ep(oto_server_tcp_t *server, endpoint_socket_t *ep);

#endif /* _CHATS_ONE_TO_ONE_SERVER_H_ */
