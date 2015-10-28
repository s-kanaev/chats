#ifndef _CHATS_CONNECTION_H_
# define _CHATS_CONNECTION_H_

# include "endpoint.h"
# include "io-service.h"
# include "network.h"

typedef struct connection {
    void *host;
    endpoint_socket_t ep_skt;
} connection_t;

void connection_send_recv_sync(connection_t *connection,
                               struct send_recv_tcp_buffer *srb);
void connection_send_recv_async(connection_t *connection,
                                struct send_recv_tcp_buffer *srb,
                                io_service_t *iosvc);

#endif /* _CHATS_CONNECTION_H_ */