#ifndef _CHATS_CONNECTION_H_
# define _CHATS_CONNECTION_H_

# include "endpoint.h"
# include "io-service.h"
# include "network.h"

typedef struct connection {
    void *host;
    endpoint_socket_t ep_skt;
} connection_t;

#endif /* _CHATS_CONNECTION_H_ */