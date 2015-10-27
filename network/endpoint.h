#ifndef _CHATS_NETWORK_H_
# define _CHATS_NETWORK_H_

# include <stdint.h>
# include <netinet/in.h>

typedef enum endpoint_class_enum {
    EPC_IP4,
    EPC_IP6,
    EPC_MAX,
    EPC_NONE = EPC_MAX
} endpoint_class_t;

typedef enum endpoint_type_enum {
    EPT_TCP,
    EPT_UDP,
    EPT_MAX,
    EPT_NONE = EPT_MAX
} endpoint_type_t;

struct endpoint;
typedef struct endpoint endpoint_t;

struct endpoint_socket;
typedef struct endpoint_socket endpoint_socket_t;

struct ip4_endpoint;
typedef struct ip4_endpoint ip4_endpoint_t;

struct ip6_endpoint;
typedef struct ip6_endpoint ip6_endpoint_t;

struct ip4_endpoint {
    uint8_t addr[4];
    uint16_t port;
};

struct ip6_endpoint {
    uint8_t addr[16];
    uint16_t port;
};

struct endpoint {
    endpoint_class_t ep_class;
    endpoint_type_t ep_type;
    union {
        ip4_endpoint_t ip4;
        ip6_endpoint_t ip6;
    } ep;
    union {
        struct sockaddr_in ip4;
        struct sockaddr_in6 ip6;
    } addr;
};

struct endpoint_socket {
    int skt;
    endpoint_t ep;
};

#endif /* _CHATS_NETWORK_H_ */
