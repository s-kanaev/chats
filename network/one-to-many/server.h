#ifndef _CHATS_ONE_TO_MANY_H_
# define _CHATS_ONE_TO_MANY_H_

# include "network.h"
# include "endpoint.h"
# include "io-service.h"
# include "memory.h"

# define DEFAULT_CONNECTION_BACKLOG 50

/* otm stands for one-to-many */
struct otm_server_tcp;
typedef struct otm_server_tcp otm_server_tcp_t;

otm_server_tcp_t *otm_server_tcp_init(io_service_t *svc,
                                      const char *addr, const char *port,
                                      int connection_backlog,
                                      int reuse_addr);
void otm_server_tcp_deinit(otm_server_tcp_t *server);
void otm_server_tcp_disconnect(otm_server_tcp_t *server,
                               const connection_t *connection);

void otm_server_tcp_listen_sync(otm_server_tcp_t *server,
                                tcp_connection_cb_t cb, void *ctx);
void otm_server_tcp_listen_async(otm_server_tcp_t *server,
                                 tcp_connection_cb_t cb, void *ctx);

void otm_server_tcp_send_sync(otm_server_tcp_t *server,
                              const connection_t *connection,
                              buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx);

void otm_server_tcp_send_async(otm_server_tcp_t *server,
                               const connection_t *connection,
                               buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx);

void otm_server_tcp_recv_sync(otm_server_tcp_t *server,
                              const connection_t *connection,
                              buffer_t *buffer,
                              network_send_recv_cb_t cb, void *ctx);

void otm_server_tcp_recv_async(otm_server_tcp_t *server,
                               const connection_t *connection,
                               buffer_t *buffer,
                               network_send_recv_cb_t cb, void *ctx);

void otm_server_tcp_local_ep(otm_server_tcp_t *server, endpoint_socket_t *ep);

#endif /* _CHATS_ONE_TO_MANY_H_ */