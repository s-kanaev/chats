#ifndef _CHATS_ONE_TO_ONE_SERVER_H_
# define _CHATS_ONE_TO_ONE_SERVER_H_

# include "endpoint.h"
# include "io-service.h"
# include "memory.h"

/* oto stands for one-to-one */

/** callback on connection accept
 * \param [in] ep connected remote endpoint
 * \param [in] ctx user context
 * \param [in] err errno
 * \return \c true until the connection is rejected
 */
typedef bool (*oto_connection_cb_t)(endpoint_t *ep, int err, void *ctx);
/** callback on data sent
 * \param [in] err errno
 * \param [in] bytes bytes sent
 * \param [in] buffer buffer sent
 * \param [in] ctx user context
 */
typedef void (*oto_send_recv_cb_t)(int err, size_t bytes, buffer_t *buffer, void *ctx);

struct oto_server_tcp;
typedef struct oto_server_tcp oto_server_tcp_t;

oto_server_tcp_t *oto_server_tcp_init(io_service_t *svc,
                                      endpoint_class_t epc,
                                      int reuse_addr,
                                      const char *local_addr, unsigned local_port);
void oto_server_tcp_deinit(oto_server_tcp_t *server);
void oto_server_tcp_listen_sync(oto_server_tcp_t *server,
                                oto_connection_cb_t cb, void *ctx);
void oto_server_tcp_listen_async(oto_server_tcp_t *server,
                                 oto_connection_cb_t cb, void *ctx);
void oto_server_tcp_disconnect(oto_server_tcp_t *server);
void oto_server_tcp_send_sync(oto_server_tcp_t *server, buffer_t *buffer,
                              oto_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_send_async(oto_server_tcp_t *server, buffer_t *buffer,
                               oto_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_recv_sync(oto_server_tcp_t *server, buffer_t *buffer,
                              oto_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_recv_async(oto_server_tcp_t *server, buffer_t *buffer,
                               oto_send_recv_cb_t cb, void *ctx);
void oto_server_tcp_local_ep(oto_server_tcp_t *server, endpoint_socket_t *ep);
void oto_server_tcp_remote_ep(oto_server_tcp_t *server, endpoint_socket_t *ep);

#endif /* _CHATS_ONE_TO_ONE_SERVER_H_ */
